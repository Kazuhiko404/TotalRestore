#include "LimbToughness.h"
#include "CriticalSection.h"
#include "ExternalModCompat.h"
#include "LimbBaselineCache.h"
#include "LimbMaxOwnership.h"
#include "ToughnessScaling.h"

#include <kenshi/RaceData.h>
#include <math.h>

/*
 * LimbToughness Module
 * Writes the toughness bonus into HealthPartStatus::_maxHealth:
 *   Scaled  (default): _maxHealth = base + LimbBonus(base, s)
 *   Percent (legacy) : _maxHealth = base * min(1 + T * HealthPercentPerToughness / 100, MaxToughnessMultiplier)
 *
 * Why the field and not the maxHealth() getter: the native
 * MedicalSystem::validateHealthValues clamps flesh against the raw
 * `_maxHealth * age * HPMult - wear`, so a getter-only buff disagrees with the
 * healing target, and saves made with the original LimbToughness carry a baked
 * `_maxHealth` that a getter multiplies again (observed 220 -> 530).
 *
 * The unbuffed base is captured once per (part GameData, race) from the `_max`
 * argument of HealthPartStatus::setup (first insert wins, like the original), so
 * a baked value loaded from an old save is replaced by base + bonus on load.
 * Parts that are not scaled (robots/animals with their switch off) are written
 * back to that captured base, so a bonus baked into a save while they were
 * buffed is removed; parts with no captured base (prosthetics) stay untouched.
 * Re-applied after MedicalSystem::init, MedicalSystem::load and Toughness XP.
 * MedicalSystem::updateStats is deliberately NOT hooked (that hook, plus a
 * floorf on flesh, is what pinned vitals in the original HealingFactor +
 * LimbToughness pair). Flesh is rescaled proportionally without flooring.
 *
 * Coexistence (YieldMaxHealthToOtherMods, only while an external max HP mod such
 * as DesolationSystem is loaded): the value written per part is recorded in
 * LimbMaxOwnership; a part whose max another mod has rewritten since is left to
 * that mod (no write, no rescale) until native setup or load writes it again.
 */

namespace TotalRestore
{
    // Changes smaller than this are float noise; skipping them avoids drifting flesh.
    static const float kMaxChangeEpsilon = 0.001f;

    // The cache is written from load/init (possibly a loader thread) and read by the healing tick.
    static LimbBaselineCache g_baselines;
    static LimbMaxOwnership g_ownership; // guarded by g_baselinesLock too
    static CriticalSection g_baselinesLock;
    static bool g_loggedYield = false;

    // Race passed to the MedicalSystem::init currently running on this thread, so
    // HealthPartStatus::setup (called from inside init) can key the baseline
    // without touching a character that is still being built.
    static __declspec(thread) GameData* t_initRace = NULL;
    static __declspec(thread) MedicalSystem* t_initMedical = NULL;

    static void (*g_origSetup)(MedicalSystem::HealthPartStatus*, GameData*, float, float, float, MedicalSystem*, bool, Character*) = NULL;
    static void (*g_origMedicalInit)(MedicalSystem*, AnimationClass*, Character*, GameData*, CharStats*) = NULL;
    static void (*g_origMedicalLoad)(MedicalSystem*, GameData*) = NULL;
    static void (*g_origXpEventBased)(CharStats*, StatsEnumerated, float) = NULL;
    static void (*g_origXpTimeBased)(CharStats*, StatsEnumerated) = NULL;

    LimbMaxSettings BuildLimbMaxSettings()
    {
        LimbMaxSettings settings;
        settings.formula = g_config.limbBonusFormula;
        settings.toughnessCap = g_config.toughnessCap;
        settings.bonus.atReference = g_config.limbBonusAtReference;
        settings.bonus.referenceHealth = g_config.limbBonusReferenceHealth;
        settings.bonus.exponent = g_config.limbBonusExponent;
        settings.bonus.maxPerPart = g_config.limbBonusMaxPerPart;
        settings.healthPercentPerToughness = g_config.healthPercentPerToughness;
        settings.maxToughnessMultiplier = g_config.maxToughnessMultiplier;
        return settings;
    }

    static const void* RaceKeyOf(Character* character)
    {
        if (character == NULL) return NULL;
        RaceData* race = character->_NV_getRace();
        return (race != NULL) ? race->data : NULL;
    }

    static LimbBaselineKey KeyFor(const MedicalSystem::HealthPartStatus* part, const void* raceKey)
    {
        // A prosthetic gets its own entry: its max comes from the limb item, not from setup.
        return LimbBaselineKey(part->data, raceKey, part->robotLimb);
    }

    static float ToughnessOf(Character* character, MedicalSystem* medical)
    {
        CharStats* stats = (character != NULL && character->stats != NULL) ? character->stats
                         : (medical != NULL ? medical->stats : NULL);
        return (stats != NULL) ? stats->toughness() : 0.0f;
    }

    static bool ShouldScale(MedicalSystem::HealthPartStatus* part, Character* character)
    {
        const bool isAnimal = (character != NULL && character->isAnimal() != NULL);
        return LimbMaxRules::ShouldScalePart(part->isRobotic(), isAnimal,
            g_config.applyToughnessToRobots, g_config.applyToughnessToAnimals);
    }

    // The base max to apply for this part, or <= 0 when it must be left untouched.
    static float BaseMaxFor(MedicalSystem::HealthPartStatus* part, const void* raceKey, bool scaled)
    {
        ScopedLock lock(g_baselinesLock);
        // Scaled: when setup was never seen for this key, the current max becomes the base.
        if (scaled) return g_baselines.Remember(KeyFor(part, raceKey), part->_maxHealth);

        // Not scaled: only a base captured from setup is trusted. Remembering the current max
        // here would capture a bonus baked into an older save. Prosthetics are never captured,
        // so they stay untouched.
        float base = 0.0f;
        return g_baselines.Find(KeyFor(part, raceKey), base) ? base : 0.0f;
    }

    // Writes a new raw max and rescales flesh proportionally (no flooring).
    static void WriteLimbMax(MedicalSystem::HealthPartStatus* part, float newMax)
    {
        const float oldMax = part->_maxHealth;
        if (fabsf(newMax - oldMax) < kMaxChangeEpsilon) return;

        part->flesh = ToughnessScaling::RescaleFlesh(part->flesh, oldMax, newMax);
        part->_maxHealth = newMax;
        part->updateDerivedHealths();
    }

    // True when another mod rewrote this part's max since our last write (LimbMaxOwnership.h).
    static bool IsYieldedToOtherMod(MedicalSystem::HealthPartStatus* part, Character* character)
    {
        bool yielded = false;
        {
            ScopedLock lock(g_baselinesLock);
            yielded = g_ownership.IsForeignOwned(part, part->_maxHealth, part->robotLimb);
        }
        if (yielded && !g_loggedYield)
        {
            g_loggedYield = true;
            Log("LimbToughness: another mod rewrote a limb max HP (first seen on " + character->getName() +
                "); TotalRestore leaves the max of such parts to it (YieldMaxHealthToOtherMods=1).");
        }
        return yielded;
    }

    static void RecordOurMax(MedicalSystem::HealthPartStatus* part)
    {
        ScopedLock lock(g_baselinesLock);
        g_ownership.RecordWrite(part, part->_maxHealth, part->robotLimb);
    }

    // Native setup / load write _maxHealth themselves: the field is ours to apply again.
    static void ReleaseOwnership(MedicalSystem* medical)
    {
        ScopedLock lock(g_baselinesLock);
        const uint32_t anatomyCount = (uint32_t)medical->anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            g_ownership.Release(medical->anatomy[i]);
        }
    }

    void ApplyLimbToughness(MedicalSystem* medical)
    {
        if (!g_config.enableLimbToughness || medical == NULL) return;

        Character* character = medical->me;
        if (character == NULL) return;

        const LimbMaxSettings settings = BuildLimbMaxSettings();
        const float toughness = ToughnessOf(character, medical);
        const void* raceKey = RaceKeyOf(character);
        const bool yieldToOtherMods = IsMaxHealthYieldActive();

        const uint32_t anatomyCount = (uint32_t)medical->anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            MedicalSystem::HealthPartStatus* part = medical->anatomy[i];
            if (part == NULL) continue;

            // Checked before BaseMaxFor, which would otherwise capture the foreign max as our base.
            if (yieldToOtherMods && IsYieldedToOtherMod(part, character)) continue;

            const bool scaled = ShouldScale(part, character);
            const float base = BaseMaxFor(part, raceKey, scaled);
            if (!(base > 0.0f)) continue;

            WriteLimbMax(part, LimbMaxRules::TargetLimbMax(scaled, base, toughness, settings));
            RecordOurMax(part);
        }
    }

    float ResolveLimbBaseMax(MedicalSystem::HealthPartStatus* part)
    {
        if (part == NULL) return 0.0f;

        const float current = part->_maxHealth;
        if (!g_config.enableLimbToughness) return current;

        MedicalSystem* medical = part->medical;
        Character* character = (medical != NULL && medical->me != NULL) ? medical->me : part->me;
        if (!ShouldScale(part, character)) return current;

        float base = 0.0f;
        {
            ScopedLock lock(g_baselinesLock);
            if (g_baselines.Find(KeyFor(part, RaceKeyOf(character)), base)) return base;
        }
        return LimbMaxRules::EstimateBaseFromMax(current, ToughnessOf(character, medical), BuildLimbMaxSettings());
    }

    static void HealthPartStatus_setup_hook(MedicalSystem::HealthPartStatus* thisptr, GameData* dat, float hitChance,
        float maxValue, float age, MedicalSystem* med, bool selfHeal, Character* me)
    {
        g_origSetup(thisptr, dat, hitChance, maxValue, age, med, selfHeal, me);

        if (dat == NULL) return;
        const void* raceKey = (t_initMedical != NULL && t_initMedical == med) ? t_initRace : RaceKeyOf(me);

        ScopedLock lock(g_baselinesLock);
        g_baselines.Remember(LimbBaselineKey(dat, raceKey, NULL), maxValue);
        g_ownership.Release(thisptr);
    }

    static void MedicalSystem_init_hook(MedicalSystem* thisptr, AnimationClass* animsys, Character* c, GameData* race, CharStats* stats)
    {
        GameData* previousRace = t_initRace;
        MedicalSystem* previousMedical = t_initMedical;
        t_initRace = race;
        t_initMedical = thisptr;

        g_origMedicalInit(thisptr, animsys, c, race, stats);

        t_initRace = previousRace;
        t_initMedical = previousMedical;
        ApplyLimbToughness(thisptr);
    }

    static void MedicalSystem_load_hook(MedicalSystem* thisptr, GameData* state)
    {
        g_origMedicalLoad(thisptr, state);
        if (thisptr != NULL) ReleaseOwnership(thisptr);
        ApplyLimbToughness(thisptr);
    }

    static void ApplyAfterToughnessXp(CharStats* stats, StatsEnumerated stat)
    {
        if (stat != STAT_TOUGHNESS || stats == NULL) return;
        MedicalSystem* medical = (stats->medical != NULL) ? stats->medical
                               : (stats->me != NULL ? &stats->me->medical : NULL);
        ApplyLimbToughness(medical);
    }

    static void CharStats_xpStat_eventBased_hook(CharStats* thisptr, StatsEnumerated stat, float amount)
    {
        g_origXpEventBased(thisptr, stat, amount);
        ApplyAfterToughnessXp(thisptr, stat);
    }

    static void CharStats_xpStat_timeBased_hook(CharStats* thisptr, StatsEnumerated stat)
    {
        g_origXpTimeBased(thisptr, stat);
        ApplyAfterToughnessXp(thisptr, stat);
    }

    template <typename Fn>
    static bool InstallHook(const char* label, intptr_t target, Fn detour, Fn* original)
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(target, (void*)detour, original))
        {
            LogError(std::string("Could not install ") + label + " hook for LimbToughness.");
            return false;
        }
        return true;
    }

    bool InitLimbToughness()
    {
        bool ok = true;
        ok &= InstallHook("HealthPartStatus::setup",
            KenshiLib::GetRealAddress(&MedicalSystem::HealthPartStatus::setup), HealthPartStatus_setup_hook, &g_origSetup);
        ok &= InstallHook("MedicalSystem::init",
            KenshiLib::GetRealAddress(&MedicalSystem::init), MedicalSystem_init_hook, &g_origMedicalInit);
        ok &= InstallHook("MedicalSystem::load",
            KenshiLib::GetRealAddress(&MedicalSystem::load), MedicalSystem_load_hook, &g_origMedicalLoad);
        ok &= InstallHook("CharStats::xpStat_eventBased",
            KenshiLib::GetRealAddress(&CharStats::xpStat_eventBased), CharStats_xpStat_eventBased_hook, &g_origXpEventBased);
        ok &= InstallHook("CharStats::xpStat_timeBased",
            KenshiLib::GetRealAddress(&CharStats::xpStat_timeBased), CharStats_xpStat_timeBased_hook, &g_origXpTimeBased);

        Log(ok ? "LimbToughness hooks installed successfully." : "LimbToughness started with missing hooks (see errors above).");
        return ok;
    }
}
