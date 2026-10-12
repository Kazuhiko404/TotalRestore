#include "HealingEngine.h"
#include "LimbRegrowth.h"

/*
 * DamageSignals Module
 * Reports damage and character removal to the healing engine, like HealingFactor:
 *   - HealthPartStatus::applyDamage (combat hits); also snapshots intact limbs for regrowth first
 *   - MedicalSystem environmental damage: gettingEaten, gettingAcidRain,
 *     gettingAcidWater, gettingAcidFeet, gettingGassed, gettingWindyface, gettingBurnt
 *   - GameWorld::removeFromUpdateListMain (drops per-character state)
 * All seven environmental functions exist in KenshiLib 0.5.0; a hook that fails
 * to install is logged and the others keep working.
 */

namespace TotalRestore
{
    typedef bool (*EnvironmentalDamageFn)(MedicalSystem*, float);

    static void (*g_origApplyDamage)(MedicalSystem::HealthPartStatus*, const Damages&) = NULL;
    static bool (*g_origGettingEaten)(MedicalSystem*, float, bool) = NULL;
    static EnvironmentalDamageFn g_origAcidRain = NULL;
    static EnvironmentalDamageFn g_origAcidWater = NULL;
    static EnvironmentalDamageFn g_origAcidFeet = NULL;
    static EnvironmentalDamageFn g_origGassed = NULL;
    static EnvironmentalDamageFn g_origWindyface = NULL;
    static EnvironmentalDamageFn g_origBurnt = NULL;
    static void (*g_origRemoveFromUpdateListMain)(GameWorld*, Character*) = NULL;

    static void ReportEnvironmentalDamage(MedicalSystem* medical, float amount)
    {
        if (medical != NULL && amount > 0.0f)
        {
            NotifyCharacterDamaged(medical->me);
        }
    }

    static void HealthPartStatus_applyDamage_hook(MedicalSystem::HealthPartStatus* thisptr, const Damages& dmg)
    {
        // Limb regrowth: snapshot the intact limb bones before a hit can sever one.
        if (thisptr != NULL && thisptr->medical != NULL)
        {
            CaptureIntactLimbs(thisptr->medical->me);
        }

        g_origApplyDamage(thisptr, dmg);

        if (thisptr != NULL && thisptr->medical != NULL)
        {
            NotifyCharacterDamaged(thisptr->medical->me);
        }
    }

    static bool MedicalSystem_gettingEaten_hook(MedicalSystem* thisptr, float amount, bool vampire)
    {
        const bool result = g_origGettingEaten(thisptr, amount, vampire);
        ReportEnvironmentalDamage(thisptr, amount);
        return result;
    }

    // One detour per environmental function (each needs its own original pointer).
#define TR_ENVIRONMENTAL_HOOK(hookName, original)                              \
    static bool hookName(MedicalSystem* thisptr, float amount)                 \
    {                                                                          \
        const bool result = original(thisptr, amount);                         \
        ReportEnvironmentalDamage(thisptr, amount);                            \
        return result;                                                         \
    }

    TR_ENVIRONMENTAL_HOOK(MedicalSystem_gettingAcidRain_hook, g_origAcidRain)
    TR_ENVIRONMENTAL_HOOK(MedicalSystem_gettingAcidWater_hook, g_origAcidWater)
    TR_ENVIRONMENTAL_HOOK(MedicalSystem_gettingAcidFeet_hook, g_origAcidFeet)
    TR_ENVIRONMENTAL_HOOK(MedicalSystem_gettingGassed_hook, g_origGassed)
    TR_ENVIRONMENTAL_HOOK(MedicalSystem_gettingWindyface_hook, g_origWindyface)
    TR_ENVIRONMENTAL_HOOK(MedicalSystem_gettingBurnt_hook, g_origBurnt)
#undef TR_ENVIRONMENTAL_HOOK

    static void GameWorld_removeFromUpdateListMain_hook(GameWorld* thisptr, Character* character)
    {
        if (character != NULL)
        {
            ForgetHealingState(character);
        }
        g_origRemoveFromUpdateListMain(thisptr, character);
    }

    template <typename Fn>
    static bool InstallHook(const char* label, intptr_t target, Fn detour, Fn* original)
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(target, (void*)detour, original))
        {
            LogError(std::string("Could not install ") + label + " hook for HealingEngine (damage pause from it is unavailable).");
            return false;
        }
        return true;
    }

    bool InitDamageSignals()
    {
        bool ok = true;
        ok &= InstallHook("HealthPartStatus::applyDamage",
            KenshiLib::GetRealAddress(&MedicalSystem::HealthPartStatus::applyDamage), HealthPartStatus_applyDamage_hook, &g_origApplyDamage);
        ok &= InstallHook("MedicalSystem::gettingEaten",
            KenshiLib::GetRealAddress(&MedicalSystem::gettingEaten), MedicalSystem_gettingEaten_hook, &g_origGettingEaten);
        ok &= InstallHook("MedicalSystem::gettingAcidRain",
            KenshiLib::GetRealAddress(&MedicalSystem::gettingAcidRain), MedicalSystem_gettingAcidRain_hook, &g_origAcidRain);
        ok &= InstallHook("MedicalSystem::gettingAcidWater",
            KenshiLib::GetRealAddress(&MedicalSystem::gettingAcidWater), MedicalSystem_gettingAcidWater_hook, &g_origAcidWater);
        ok &= InstallHook("MedicalSystem::gettingAcidFeet",
            KenshiLib::GetRealAddress(&MedicalSystem::gettingAcidFeet), MedicalSystem_gettingAcidFeet_hook, &g_origAcidFeet);
        ok &= InstallHook("MedicalSystem::gettingGassed",
            KenshiLib::GetRealAddress(&MedicalSystem::gettingGassed), MedicalSystem_gettingGassed_hook, &g_origGassed);
        ok &= InstallHook("MedicalSystem::gettingWindyface",
            KenshiLib::GetRealAddress(&MedicalSystem::gettingWindyface), MedicalSystem_gettingWindyface_hook, &g_origWindyface);
        ok &= InstallHook("MedicalSystem::gettingBurnt",
            KenshiLib::GetRealAddress(&MedicalSystem::gettingBurnt), MedicalSystem_gettingBurnt_hook, &g_origBurnt);
        ok &= InstallHook("GameWorld::removeFromUpdateListMain",
            KenshiLib::GetRealAddress(&GameWorld::removeFromUpdateListMain), GameWorld_removeFromUpdateListMain_hook, &g_origRemoveFromUpdateListMain);

        if (ok) Log("Damage signal hooks installed successfully.");
        return ok;
    }
}
