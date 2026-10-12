#include "LimbRegrowth.h"
#include "HealingEngine.h"
#include "LimbBoneSnapshot.h"
#include "LimbRegrowthState.h"
#include "LimbShadowHooks.h"
#include "LimbToughness.h"
#include "MedicalUpdateHook.h"
#include "ToughnessScaling.h"

#include <kenshi/RaceData.h>
#include <ogre/OgreVector3.h>

/*
 * LimbRegrowth Module: faithful port of HealingFactor's organic limb regrowth.
 *
 * amputate (hook) -> beginRegen (also DetectStumps for stumps from saves)
 *   -> reveal: setLimb(ORIGINAL) so the organic mesh exists, then "shadow" state
 *      (raw ORIGINAL, getters report STUMP + stashed item; LimbShadowHooks.cpp)
 *   -> growth: Bip01 limb bones scaled from InitialScale to the captured size
 *   -> complete: bones restored to the snapshot, setLimb(ORIGINAL) for gameplay,
 *      completion flesh = min(CompletionHealthFraction * max, regen ceiling)
 *      (Legacy healing formula: the fraction only).
 * stop: robotic limb installed (raw LIMB_REPLACED), state changed, disabled.
 *
 * TotalRestore differences from HealingFactor: humanoids only and skeleton races
 * (RaceData::robot) never regrow; growth pauses with the healing pauses (damage,
 * combat, knock-out); the completion flesh respects the Toughness regen ceiling.
 * Blood, bleed rate and the part's health are preserved around every setLimb.
 */

namespace TotalRestore
{
    static const int kLimbCount = 4;

    static bool g_regrowthActive = false;
    static void (*g_origAmputate)(MedicalSystem*, RobotLimbs::Limb, bool, const Ogre::Vector3&) = NULL;

    RegrowthSettings BuildRegrowthSettings()
    {
        RegrowthSettings settings;
        settings.enabled = g_config.enableMod && g_config.enableLimbRegeneration;
        settings.limbEnabled[0] = g_config.enableLeftArmRegeneration;
        settings.limbEnabled[1] = g_config.enableRightArmRegeneration;
        settings.limbEnabled[2] = g_config.enableLeftLegRegeneration;
        settings.limbEnabled[3] = g_config.enableRightLegRegeneration;
        settings.regenerationSeconds = g_config.limbRegenerationSeconds;
        settings.armRegenerationSeconds = g_config.armRegenerationSeconds;
        settings.legRegenerationSeconds = g_config.legRegenerationSeconds;
        settings.initialScale = g_config.initialScale;
        settings.armInitialScale = g_config.armInitialScale;
        settings.legInitialScale = g_config.legInitialScale;
        settings.completionHealthFraction = g_config.completionHealthFraction;
        settings.armCompletionHealthFraction = g_config.armCompletionHealthFraction;
        settings.legCompletionHealthFraction = g_config.legCompletionHealthFraction;
        settings.smoothCurve = RegrowthRules::ParseSmoothCurve(g_config.limbGrowthCurve.c_str());
        return settings;
    }

    bool IsLimbRegrowthActive()
    {
        return g_regrowthActive;
    }

    static const char* LimbName(int limb)
    {
        static const char* names[kLimbCount] = { "left arm", "right arm", "left leg", "right leg" };
        return (limb >= 0 && limb < kLimbCount) ? names[limb] : "limb";
    }

    static void LogLimb(Character* character, int limb, const char* what)
    {
        Log(std::string("Regrowth: ") + LimbName(limb) + " of " + character->getName() + ": " + what);
    }

    // Humanoid player characters only; skeleton races never regrow.
    static bool IsRegrowthEligible(Character* character)
    {
        if (!IsHealingEligible(character) || character->isHuman() == NULL) return false;
        RaceData* race = character->_NV_getRace();
        return race == NULL || !race->robot;
    }

    static float ReferenceMax(MedicalSystem* medical)
    {
        float reference = 0.0f;
        const uint32_t anatomyCount = (uint32_t)medical->anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            MedicalSystem::HealthPartStatus* part = medical->anatomy[i];
            if (part == NULL || part->isRobotic() || RegrowthState::LimbIndexOfPart(medical, part) >= 0) continue;
            if (part->_maxHealth > reference) reference = part->_maxHealth;
        }
        return reference;
    }

    // setLimb must not heal the limb or reset blood: everything it touches is put back.
    struct SetLimbSideEffects
    {
        float blood, bleedRate, extraBloodLoss;

        explicit SetLimbSideEffects(const MedicalSystem* medical)
            : blood(medical->blood)
            , bleedRate(medical->currentBleedRate)
            , extraBloodLoss(medical->extraBloodLossFromBodyparts)
        {
        }

        void Restore(MedicalSystem* medical) const
        {
            medical->blood = blood;
            medical->currentBleedRate = bleedRate;
            medical->extraBloodLossFromBodyparts = extraBloodLoss;
        }
    };

    // Returns true when a shadow was dropped (the raw state goes back to the stump).
    static bool DropShadow(RobotLimbs* robotLimbs, int limb, LimbEntry& entry)
    {
        const bool hadShadow = entry.shadow;
        if (hadShadow && robotLimbs != NULL && robotLimbs->states[limb] == LIMB_ORIGINAL)
        {
            robotLimbs->states[limb] = LIMB_STUMP;
            robotLimbs->items[limb] = entry.stash;
        }
        RegrowthState::SetShadow(entry, false);
        entry.stash = NULL;
        return hadShadow;
    }

    static void StopRegen(Character* character, int limb, LimbEntry& entry, const char* reason)
    {
        if (entry.snapValid) LimbBoneSnapshot::Restore(character, entry);
        const bool dropped = DropShadow(character->medical.robotLimbs, limb, entry);
        entry.active = false;
        LogLimb(character, limb, reason);
        if (dropped) RefreshLimbVisual(character);
    }

    static void BeginRegen(Character* character, int limb, bool restoreBones, float savedMax)
    {
        const RegrowthSettings settings = BuildRegrowthSettings();
        if (!RegrowthRules::IsLimbEnabled(settings, limb) || !IsRegrowthEligible(character)) return;

        LimbEntry& entry = RegrowthState::GetOrCreate(character).limbs[limb];
        if (entry.active) return;

        entry.active = true;
        entry.visualStarted = false;
        RegrowthState::SetShadow(entry, false);
        entry.stash = NULL;
        entry.elapsed = 0.0f;
        entry.logDeferred = false;
        if (savedMax > 0.0f) entry.savedMax = savedMax;
        if (restoreBones && entry.snapValid) LimbBoneSnapshot::Restore(character, entry);
        LogLimb(character, limb, "regrowth started");
    }

    static bool Reveal(Character* character, int limb, LimbEntry& entry)
    {
        MedicalSystem* medical = &character->medical;
        RobotLimbs* robotLimbs = medical->robotLimbs;
        if (robotLimbs == NULL || robotLimbs->states[limb] != LIMB_STUMP) return false;

        MedicalSystem::HealthPartStatus* part = RegrowthState::LimbPart(medical, limb);
        const SetLimbSideEffects blood(medical);
        PartHealthSnapshot health;
        if (part != NULL) health.Save(part);

        entry.stash = robotLimbs->items[limb];
        robotLimbs->setLimb((RobotLimbs::Limb)limb, LIMB_ORIGINAL, NULL);
        const bool ok = (robotLimbs->states[limb] == LIMB_ORIGINAL);
        if (ok)
        {
            RegrowthState::SetShadow(entry, true);
            robotLimbs->states[limb] = LIMB_ORIGINAL;
            robotLimbs->items[limb] = NULL;
        }

        blood.Restore(medical);
        if (part != NULL) health.Restore(part);
        RefreshLimbVisual(character);
        return ok;
    }

    static void RefreshAfterCompletion(Character* character)
    {
        MedicalSystem* medical = &character->medical;
        if (character->inventory == NULL || medical->robotLimbs == NULL) return;

        const float bloodBefore = medical->blood;
        medical->validateHealthValues();
        medical->_NV_updateStats();
        if (medical->blood > bloodBefore) medical->blood = bloodBefore;
        medical->reassessCollapseMode(false, false);
        if (!RegrowthState::AnyActive(character)) character->updatePortraitGUIState();

        // Arm and boot slots become usable again.
        character->validateInventorySections();
    }

    static float CompletionCeiling(Character* character, MedicalSystem::HealthPartStatus* part)
    {
        const float toughness = (character->stats != NULL) ? character->stats->toughness() : 0.0f;
        const float s = ToughnessScaling::Normalize(toughness, g_config.toughnessCap);
        return ToughnessScaling::RegenCeiling(ResolveLimbBaseMax(part), s,
            g_config.regenCeilingMinFraction, g_config.regenCeilingMaxFraction);
    }

    static void CompleteRegen(Character* character, int limb, LimbEntry& entry)
    {
        MedicalSystem* medical = &character->medical;
        RobotLimbs* robotLimbs = medical->robotLimbs;
        const RegrowthSettings settings = BuildRegrowthSettings();

        LimbBoneSnapshot::Restore(character, entry); // full captured size, not 1.0
        RegrowthState::SetShadow(entry, false);
        entry.stash = NULL;

        const SetLimbSideEffects blood(medical);
        robotLimbs->setLimb((RobotLimbs::Limb)limb, LIMB_ORIGINAL, NULL);
        blood.Restore(medical);
        robotLimbs->states[limb] = LIMB_ORIGINAL;
        robotLimbs->items[limb] = NULL;

        MedicalSystem::HealthPartStatus* part = RegrowthState::LimbPart(medical, limb);
        if (part != NULL)
        {
            if (entry.savedMax > 0.0f)
            {
                part->_maxHealth = entry.savedMax;
            }
            else
            {
                const float reference = ReferenceMax(medical);
                const float best = (reference > part->_maxHealth) ? reference : part->_maxHealth;
                if (best > 0.0f) part->_maxHealth = best;
            }
            part->robotLimb = NULL;

            // Re-normalise the max to base + toughness bonus (no-op when LimbToughness is off).
            ApplyLimbToughness(medical);

            const bool useCeiling = (g_config.healingFormula == HEALING_FORMULA_TOUGHNESS);
            part->flesh = RegrowthRules::CompletionFlesh(RegrowthRules::CompletionFraction(settings, limb),
                part->maxHealth(), useCeiling ? CompletionCeiling(character, part) : 0.0f, useCeiling);
            part->fleshStun = 0.0f;
            part->bandaging = 0.0f;
            part->juryRigging = 0.0f;
            part->wearDamage = 0.0f;
            part->updateDerivedHealths();
        }

        // Clear `active` before the refreshes so the portrait update runs and the
        // updateHiddenParts hook no longer wraps the call.
        entry.active = false;
        LogLimb(character, limb, "regrowth complete");
        RefreshLimbVisual(character);
        RefreshAfterCompletion(character);
    }

    void CaptureIntactLimbs(Character* character)
    {
        if (!g_regrowthActive || !IsRegrowthEligible(character)) return;

        MedicalSystem* medical = &character->medical;
        RobotLimbs* robotLimbs = medical->robotLimbs;
        Ogre::OldSkeletonInstance* skeleton = LimbBoneSnapshot::SkeletonOf(character);
        if (robotLimbs == NULL || skeleton == NULL) return;

        const RegrowthSettings settings = BuildRegrowthSettings();
        CharacterLimbs& limbs = RegrowthState::GetOrCreate(character);
        for (int limb = 0; limb < kLimbCount; ++limb)
        {
            if (robotLimbs->states[limb] != LIMB_ORIGINAL) continue;
            if (!RegrowthRules::IsLimbEnabled(settings, limb) &&
                !RegrowthRules::IsLimbEnabled(settings, RegrowthRules::MirrorLimb(limb))) continue;

            LimbEntry& entry = limbs.limbs[limb];
            if (entry.active) continue;
            if (!(entry.snapValid && entry.snapSkel == skeleton))
            {
                LimbBoneSnapshot::Capture(character, limb, entry);
            }

            MedicalSystem::HealthPartStatus* part = RegrowthState::LimbPart(medical, limb);
            if (part != NULL && part->_maxHealth > 0.0f) entry.savedMax = part->_maxHealth;
        }
    }

    static void DetectStumps(Character* character)
    {
        RobotLimbs* robotLimbs = character->medical.robotLimbs;
        if (robotLimbs == NULL) return;
        for (int limb = 0; limb < kLimbCount; ++limb)
        {
            // Stumps from saves and pre-existing stumps restart from 0 (nothing is persisted).
            if (robotLimbs->states[limb] == LIMB_STUMP) BeginRegen(character, limb, false, 0.0f);
        }
    }

    static void RepairCollapsedLimbMax(Character* character)
    {
        MedicalSystem* medical = &character->medical;
        RobotLimbs* robotLimbs = medical->robotLimbs;
        if (robotLimbs == NULL) return;

        const float reference = ReferenceMax(medical);
        for (int limb = 0; limb < kLimbCount; ++limb)
        {
            if (robotLimbs->states[limb] != LIMB_ORIGINAL || RegrowthState::ShadowEntry(character, limb) != NULL) continue;
            MedicalSystem::HealthPartStatus* part = RegrowthState::LimbPart(medical, limb);
            if (part == NULL || part->isRobotic() || !RegrowthRules::NeedsMaxRepair(part->_maxHealth, reference)) continue;

            part->_maxHealth = reference;
            part->updateDerivedHealths();
        }
    }

    // Returns false when growth must wait (no snapshot anywhere yet).
    static bool EnsureSnapshot(Character* character, int limb, CharacterLimbs& limbs)
    {
        LimbEntry& entry = limbs.limbs[limb];
        const int mirror = RegrowthRules::MirrorLimb(limb);
        const SnapshotSource source = RegrowthRules::ChooseSnapshotSource(entry.snapValid, limbs.limbs[mirror].snapValid);
        if (source == SNAPSHOT_OWN) return true;
        if (source == SNAPSHOT_MIRROR && LimbBoneSnapshot::CopyFromMirror(character, limb, limbs.limbs[mirror], entry)) return true;

        if (!entry.logDeferred)
        {
            entry.logDeferred = true;
            LogLimb(character, limb, "regrowth deferred: no intact bone snapshot of this limb or its mirror yet");
        }
        return false;
    }

    // Appearance rebuilt: rebind to the new skeleton, or restart from a fresh snapshot.
    static void FollowSkeleton(Character* character, LimbEntry& entry)
    {
        Ogre::OldSkeletonInstance* skeleton = LimbBoneSnapshot::SkeletonOf(character);
        if (!entry.snapValid || entry.snapSkel == skeleton || LimbBoneSnapshot::Rebind(entry, skeleton)) return;

        LimbBoneSnapshot::Invalidate(entry);
        entry.elapsed = 0.0f;
    }

    static void GrowLimb(Character* character, int limb, CharacterLimbs& limbs, const RegrowthSettings& settings, float dt)
    {
        LimbEntry& entry = limbs.limbs[limb];
        if (!entry.active) return;

        if (!RegrowthRules::IsLimbEnabled(settings, limb))
        {
            StopRegen(character, limb, entry, "regrowth stopped: disabled");
            return;
        }

        // Raw read: never through the hooked getters.
        const RawStateVerdict verdict = RegrowthRules::EvaluateRawState((int)character->medical.robotLimbs->states[limb], entry.shadow);
        if (verdict == RAW_STATE_STOP_REPLACED)
        {
            StopRegen(character, limb, entry, "regrowth stopped: robotic replacement installed");
            return;
        }
        if (verdict == RAW_STATE_STOP_CHANGED)
        {
            StopRegen(character, limb, entry, "regrowth stopped: limb state changed");
            return;
        }

        if (!EnsureSnapshot(character, limb, limbs)) return;
        FollowSkeleton(character, entry);

        if (!entry.visualStarted)
        {
            if (!entry.shadow && !Reveal(character, limb, entry)) return;
            entry.visualStarted = true;
        }

        FollowSkeleton(character, entry);
        if (!entry.snapValid) return;

        const float elapsed = entry.elapsed + ((dt > 0.0f) ? dt : 0.0f);
        const float seconds = RegrowthRules::GrowthSeconds(settings, limb);
        const float scale = RegrowthRules::GrowthScale(elapsed, seconds, RegrowthRules::InitialScale(settings, limb), settings.smoothCurve);
        if (!LimbBoneSnapshot::ApplyScale(character, entry, scale)) return; // elapsed not committed

        entry.elapsed = elapsed;
        if (elapsed >= seconds) CompleteRegen(character, limb, entry);
    }

    void ProcessLimbRegrowth(Character* character, float frameTime, bool canRegrow)
    {
        if (!g_regrowthActive || character == NULL) return;

        if (!IsRegrowthEligible(character))
        {
            if (RegrowthState::Find(character) != NULL)
            {
                ForgetLimbRegrowth(character);
                RefreshLimbVisual(character);
            }
            return;
        }
        if (character->medical.robotLimbs == NULL) return;

        CaptureIntactLimbs(character);
        RepairCollapsedLimbMax(character);
        DetectStumps(character);
        if (!canRegrow) return; // paused with the healing pauses (damage, combat, knock-out)

        const RegrowthSettings settings = BuildRegrowthSettings();
        CharacterLimbs& limbs = RegrowthState::GetOrCreate(character);
        for (int limb = 0; limb < kLimbCount; ++limb)
        {
            GrowLimb(character, limb, limbs, settings, frameTime);
        }
    }

    void ForgetLimbRegrowth(Character* character)
    {
        CharacterLimbs* limbs = RegrowthState::Find(character);
        if (limbs == NULL) return;

        RobotLimbs* robotLimbs = character->medical.robotLimbs;
        for (int limb = 0; limb < kLimbCount; ++limb)
        {
            LimbEntry& entry = limbs->limbs[limb];
            if (entry.snapValid) LimbBoneSnapshot::Restore(character, entry);
            DropShadow(robotLimbs, limb, entry);
        }
        RegrowthState::Erase(character);
    }

    void ShutdownLimbRegrowth()
    {
        std::vector<Character*> characters;
        RegrowthState::CollectCharacters(characters);
        for (size_t i = 0; i < characters.size(); ++i)
        {
            ForgetLimbRegrowth(characters[i]);
        }
    }

    static void MedicalSystem_amputate_hook(MedicalSystem* thisptr, RobotLimbs::Limb limb, bool createSeveredItem, const Ogre::Vector3& force)
    {
        const int index = (int)limb;
        const RegrowthSettings settings = BuildRegrowthSettings();
        if (!g_regrowthActive || !RegrowthRules::IsLimbEnabled(settings, index) || thisptr == NULL)
        {
            g_origAmputate(thisptr, limb, createSeveredItem, force);
            return;
        }

        Character* character = thisptr->me;
        CharacterLimbs* limbs = (character != NULL) ? RegrowthState::Find(character) : NULL;
        if (limbs != NULL && limbs->limbs[index].active)
        {
            return; // HealingFactor: a regrowing limb cannot be severed again until it completes or stops
        }

        MedicalSystem::HealthPartStatus* part = RegrowthState::LimbPart(thisptr, index);
        const float savedMax = (part != NULL) ? part->_maxHealth : 0.0f;
        if (character != NULL && IsRegrowthEligible(character) && thisptr->robotLimbs != NULL &&
            thisptr->robotLimbs->states[index] == LIMB_ORIGINAL)
        {
            // A failed capture is only logged; growth later waits for a mirror snapshot.
            LimbBoneSnapshot::Capture(character, index, RegrowthState::GetOrCreate(character).limbs[index]);
        }

        g_origAmputate(thisptr, limb, createSeveredItem, force);

        if (character != NULL && thisptr->robotLimbs != NULL && thisptr->robotLimbs->states[index] == LIMB_STUMP)
        {
            BeginRegen(character, index, true, savedMax);
        }
    }

    bool InitLimbRegrowth()
    {
        if (!RegrowthRules::AnyLimbEnabled(BuildRegrowthSettings())) return false;

        if (!LimbBoneSnapshot::ResolveOgreExports())
        {
            LogError("LimbRegrowth disabled: OgreMain_x64.dll bone size exports not found.");
            return false;
        }

        if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                KenshiLib::GetRealAddress(&MedicalSystem::amputate), MedicalSystem_amputate_hook, &g_origAmputate))
        {
            LogError("Could not install MedicalSystem::amputate hook; LimbRegrowth disabled.");
            return false;
        }

        if (!InitLimbShadowHooks())
        {
            // Without every getter/visual hook the two limb truths cannot be kept consistent.
            LogError("LimbRegrowth disabled: missing shadow hooks (see errors above).");
            return false;
        }

        if (!InitMedicalUpdateHook())
        {
            LogError("LimbRegrowth disabled: the medicalUpdate hook is required.");
            return false;
        }

        g_regrowthActive = true;
        Log("LimbRegrowth hooks installed successfully.");
        return true;
    }
}
