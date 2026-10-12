#include "LimbShadowHooks.h"
#include "LimbRegrowth.h"
#include "LimbRegrowthState.h"
#include "RegrowthRules.h"

/*
 * LimbShadowHooks Module (HealingFactor regrowth port, see LimbShadowHooks.h)
 * Every hook reads the raw RobotLimbs::states/items directly; our own logic never
 * calls the hooked getters, otherwise the lie would feed back into the state checks.
 */

namespace TotalRestore
{
    static const int kLimbCount = 4;

    static LimbState (*g_origGetState)(RobotLimbs*, RobotLimbs::Limb) = NULL;
    static Item* (*g_origGetLimb)(RobotLimbs*, RobotLimbs::Limb) = NULL;
    static int (*g_origGetMask)(RobotLimbs*) = NULL;
    static void (*g_origSave)(RobotLimbs*, GameData*) = NULL;
    static LimbState (*g_origGetLimbState)(MedicalSystem*, RobotLimbs::Limb) = NULL;
    static LimbState (*g_origGetRobotLimbState)(MedicalSystem::HealthPartStatus*) = NULL;
    static bool (*g_origPartIsDead)(MedicalSystem::HealthPartStatus*) = NULL;
    static void (*g_origUpdateHiddenParts)(AppearanceHuman*) = NULL;

    static bool InsideVisualRefresh()
    {
        return RegrowthState::VisualDepth() > 0;
    }

    static LimbState ShadowState()
    {
        return InsideVisualRefresh() ? LIMB_ORIGINAL : LIMB_STUMP;
    }

    // ---- Getter hooks -------------------------------------------------------

    static LimbState RobotLimbs_getState_hook(RobotLimbs* thisptr, RobotLimbs::Limb limb)
    {
        if (RegrowthState::ShadowEntryForRobotLimbs(thisptr, (int)limb) != NULL) return ShadowState();
        return g_origGetState(thisptr, limb);
    }

    static Item* RobotLimbs_getLimb_hook(RobotLimbs* thisptr, RobotLimbs::Limb limb)
    {
        LimbEntry* entry = RegrowthState::ShadowEntryForRobotLimbs(thisptr, (int)limb);
        if (entry != NULL) return InsideVisualRefresh() ? NULL : entry->stash;
        return g_origGetLimb(thisptr, limb);
    }

    static int RobotLimbs_getMask_hook(RobotLimbs* thisptr)
    {
        int mask = g_origGetMask(thisptr);
        if (thisptr == NULL || InsideVisualRefresh()) return mask;
        for (int i = 0; i < kLimbCount; ++i)
        {
            if (RegrowthState::ShadowEntryForRobotLimbs(thisptr, i) != NULL) mask |= (1 << i);
        }
        return mask;
    }

    static LimbState MedicalSystem_getLimbState_hook(MedicalSystem* thisptr, RobotLimbs::Limb limb)
    {
        if (thisptr != NULL && RegrowthState::ShadowEntryForRobotLimbs(thisptr->robotLimbs, (int)limb) != NULL) return ShadowState();
        return g_origGetLimbState(thisptr, limb);
    }

    static LimbState HealthPartStatus_getRobotLimbState_hook(MedicalSystem::HealthPartStatus* thisptr)
    {
        if (RegrowthState::ShadowEntryForPart(thisptr) != NULL) return ShadowState();
        return g_origGetRobotLimbState(thisptr);
    }

    static bool HealthPartStatus_isDead_hook(MedicalSystem::HealthPartStatus* thisptr)
    {
        // Keep the regrowing mesh visible while the refresh decides what to hide.
        if (InsideVisualRefresh() && RegrowthState::ShadowEntryForPart(thisptr) != NULL) return false;
        return g_origPartIsDead(thisptr);
    }

    // ---- Save: the save file records a stump; regrowth restarts after a reload ----

    static void RobotLimbs_save_hook(RobotLimbs* thisptr, GameData* state)
    {
        if (thisptr == NULL)
        {
            g_origSave(thisptr, state);
            return;
        }

        LimbState states[kLimbCount];
        Item* items[kLimbCount];
        for (int i = 0; i < kLimbCount; ++i)
        {
            states[i] = thisptr->states[i];
            items[i] = thisptr->items[i];
            LimbEntry* entry = RegrowthState::ShadowEntryForRobotLimbs(thisptr, i);
            if (entry != NULL)
            {
                thisptr->states[i] = LIMB_STUMP;
                thisptr->items[i] = entry->stash;
            }
        }

        g_origSave(thisptr, state);

        for (int i = 0; i < kLimbCount; ++i)
        {
            thisptr->states[i] = states[i];
            thisptr->items[i] = items[i];
        }
    }

    // ---- medicalUpdate scope ------------------------------------------------

    ShadowStumpScope::ShadowStumpScope(MedicalSystem* medical)
        : m_robotLimbs((medical != NULL) ? medical->robotLimbs : NULL)
    {
        for (int i = 0; i < kLimbCount; ++i)
        {
            m_changed[i] = false;
            m_states[i] = LIMB_ORIGINAL;
            m_items[i] = NULL;
            if (m_robotLimbs == NULL) continue;

            m_states[i] = m_robotLimbs->states[i];
            m_items[i] = m_robotLimbs->items[i];
            LimbEntry* entry = RegrowthState::ShadowEntryForRobotLimbs(m_robotLimbs, i);
            if (entry != NULL)
            {
                m_changed[i] = true;
                m_robotLimbs->states[i] = LIMB_STUMP;
                m_robotLimbs->items[i] = entry->stash;
            }
        }
    }

    ShadowStumpScope::~ShadowStumpScope()
    {
        if (m_robotLimbs == NULL) return;
        for (int i = 0; i < kLimbCount; ++i)
        {
            // If the native code changed the limb (robotic installed), keep its value:
            // the regrowth tick then sees LIMB_REPLACED and stops.
            if (m_changed[i] && m_robotLimbs->states[i] == LIMB_STUMP)
            {
                m_robotLimbs->states[i] = m_states[i];
                m_robotLimbs->items[i] = m_items[i];
            }
        }
    }

    // ---- Visual refresh -----------------------------------------------------

    static void RefreshWithVisualTruth(Character* character, AppearanceHuman* appearance)
    {
        MedicalSystem* medical = &character->medical;
        RobotLimbs* robotLimbs = medical->robotLimbs;
        const RegrowthSettings settings = BuildRegrowthSettings();

        LimbState savedStates[kLimbCount];
        MedicalSystem::HealthPartStatus* parts[kLimbCount];
        PartHealthSnapshot fields[kLimbCount];

        for (int i = 0; i < kLimbCount; ++i)
        {
            parts[i] = NULL;
            savedStates[i] = (robotLimbs != NULL) ? robotLimbs->states[i] : LIMB_ORIGINAL;

            LimbEntry* entry = RegrowthState::ShadowEntry(character, i);
            if (entry == NULL) continue;

            // (a) a shadow limb forced back to STUMP (nested medicalUpdate) is drawn as ORIGINAL
            if (robotLimbs != NULL && robotLimbs->states[i] == LIMB_STUMP) robotLimbs->states[i] = LIMB_ORIGINAL;

            // (b) present visual health so the native refresh does not treat the limb as gone
            MedicalSystem::HealthPartStatus* part = RegrowthState::LimbPart(medical, i);
            if (part == NULL) continue;
            parts[i] = part;
            fields[i].Save(part);
            const float referenceMax = (entry->savedMax > part->maxHealth()) ? entry->savedMax : part->maxHealth();
            const float visualFlesh = RegrowthRules::CompletionFraction(settings, i) * referenceMax;
            part->flesh = (visualFlesh > 1.0f) ? visualFlesh : 1.0f;
        }

        {
            VisualDepthGuard guard;
            g_origUpdateHiddenParts(appearance);
        }

        // (c) restore the real health, (d) restore the raw states (items untouched)
        for (int i = 0; i < kLimbCount; ++i)
        {
            if (parts[i] != NULL) fields[i].Restore(parts[i]);
            if (robotLimbs != NULL) robotLimbs->states[i] = savedStates[i];
        }
    }

    void RefreshLimbVisual(Character* character)
    {
        if (character == NULL || character->isHuman() == NULL || g_origUpdateHiddenParts == NULL) return;
        AppearanceBase* appearance = character->getAppearance();
        if (appearance == NULL) return;
        RefreshWithVisualTruth(character, static_cast<AppearanceHuman*>(appearance));
    }

    static void AppearanceHuman_updateHiddenParts_hook(AppearanceHuman* thisptr)
    {
        Character* character = (thisptr != NULL) ? thisptr->me : NULL;
        if (character == NULL || !RegrowthState::AnyActive(character))
        {
            g_origUpdateHiddenParts(thisptr);
            return;
        }
        RefreshWithVisualTruth(character, thisptr);
    }

    // ---- Installation -------------------------------------------------------

    template <typename Fn>
    static bool InstallHook(const char* label, intptr_t target, Fn detour, Fn* original)
    {
        if (target == 0 || KenshiLib::SUCCESS != KenshiLib::AddHook(target, (void*)detour, original))
        {
            LogError(std::string("Could not install ") + label + " hook for LimbRegrowth.");
            return false;
        }
        return true;
    }

    static intptr_t UpdateHiddenPartsAddress()
    {
        // Private in the header; KenshiLib.dll exports it with public mangling.
        HMODULE kenshiLib = GetModuleHandleA("KenshiLib.dll");
        void* exported = (kenshiLib != NULL) ? (void*)GetProcAddress(kenshiLib, "?updateHiddenParts@AppearanceHuman@@QEAAXXZ") : NULL;
        return (exported != NULL) ? KenshiLib::GetRealAddress(exported) : 0;
    }

    bool InitLimbShadowHooks()
    {
        bool ok = true;
        ok &= InstallHook("AppearanceHuman::updateHiddenParts", UpdateHiddenPartsAddress(),
            AppearanceHuman_updateHiddenParts_hook, &g_origUpdateHiddenParts);
        ok &= InstallHook("RobotLimbs::getState",
            KenshiLib::GetRealAddress(&RobotLimbs::getState), RobotLimbs_getState_hook, &g_origGetState);
        ok &= InstallHook("RobotLimbs::getLimb",
            KenshiLib::GetRealAddress(&RobotLimbs::getLimb), RobotLimbs_getLimb_hook, &g_origGetLimb);
        ok &= InstallHook("RobotLimbs::getMask",
            KenshiLib::GetRealAddress(&RobotLimbs::getMask), RobotLimbs_getMask_hook, &g_origGetMask);
        ok &= InstallHook("RobotLimbs::save",
            KenshiLib::GetRealAddress(&RobotLimbs::save), RobotLimbs_save_hook, &g_origSave);
        ok &= InstallHook("MedicalSystem::getLimbState",
            KenshiLib::GetRealAddress(&MedicalSystem::getLimbState), MedicalSystem_getLimbState_hook, &g_origGetLimbState);
        ok &= InstallHook("HealthPartStatus::getRobotLimbState",
            KenshiLib::GetRealAddress(&MedicalSystem::HealthPartStatus::getRobotLimbState), HealthPartStatus_getRobotLimbState_hook, &g_origGetRobotLimbState);
        ok &= InstallHook("HealthPartStatus::isDead",
            KenshiLib::GetRealAddress(&MedicalSystem::HealthPartStatus::isDead), HealthPartStatus_isDead_hook, &g_origPartIsDead);
        return ok;
    }
}
