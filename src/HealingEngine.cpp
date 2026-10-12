#include "HealingEngine.h"
#include "ExternalModCompat.h"
#include "FleshCapRules.h"
#include "LimbRegrowth.h"
#include "LimbToughness.h"
#include "RegenRules.h"
#include "RegenStateRegistry.h"
#include "ToughnessScaling.h"

#include <kenshi/RaceData.h>

/*
 * HealingEngine Module
 * Hooks GameWorld::mainLoop_GPUSensitiveStuff (per-frame tick) and heals the
 * player squad: Flesh, Stun, Wear and Blood/Oil, plus limb regrowth.
 *
 * Time: dt = frameDelta * GameWorld::getFrameSpeedMultiplier() (HealingFactor),
 * no update while paused or when the multiplier is < 0 or > 100.
 *
 * Toughness formula (default), on the unbuffed base max (LimbToughness cache):
 *   rate    = base * lerp(RegenRateMin, RegenRateMax, s) % per minute
 *   ceiling = base * lerp(RegenCeilingMin, RegenCeilingMax, s)
 * Flesh only refills up to the ceiling (never lowered); stun and wear recover at
 * the same rate without a ceiling. Legacy formula: the old full-max refill over
 * WoundRegenerationSeconds with the 0.5x robotic limiter.
 *
 * Pauses: WoundHealingDelaySeconds after damage (applyDamage / environmental
 * hooks in DamageSignals.cpp, plus the HealingFactor flesh-drop watchdog), while
 * in combat, and while knocked out.
 */

namespace TotalRestore
{
    static void (*g_origMainLoop)(GameWorld*, float) = NULL;

    static RegenStateRegistry g_regenStates;

    static unsigned long long IdentityOf(Character* character)
    {
        const hand& handle = character->getHandle();
        return RegenStateRegistry::MakeIdentity(handle.index, handle.serial);
    }

    static float ClampMax(float val, float maxVal)
    {
        return (val > maxVal) ? maxVal : val;
    }

    bool IsHealingEligible(Character* character)
    {
        if (character == NULL) return false;
        if (!character->isPlayerCharacter()) return false;
        // The combat pause (isInCombatMode) misses fighting animals, so healing them would make them immortal.
        if (character->isAnimal() != NULL && !g_config.applyHealingToAnimals) return false;

        if (g_config.applyToAllPlayerHumanoids)
        {
            return true;
        }

        if (!g_config.nameSuffix.empty())
        {
            const std::string& name = character->getName();
            if (name.length() >= g_config.nameSuffix.length())
            {
                return (0 == name.compare(name.length() - g_config.nameSuffix.length(), g_config.nameSuffix.length(), g_config.nameSuffix));
            }
        }
        return false;
    }

    static bool IsKnockedOut(Character* character)
    {
        // The engine's own predicate, plus PS_KO which it misses (KENSHI-MODDING-NOTES.md).
        return character->_NV_isLiterallyUnconciousNotPretending() || character->_NV_getProneState() == PS_KO;
    }

    static bool IsRobotRace(Character* character)
    {
        RaceData* race = character->_NV_getRace();
        return (race != NULL && race->robot);
    }

    void NotifyCharacterDamaged(Character* character)
    {
        if (!IsHealingEligible(character)) return;
        g_regenStates.Get(character, IdentityOf(character)).damageCooldown = g_config.woundHealingDelaySeconds;
    }

    void ForgetHealingState(Character* character)
    {
        g_regenStates.Forget(character);
        ForgetLimbRegrowth(character);
    }

    static void RunFleshWatchdog(MedicalSystem& medical, CharacterRegenState& state)
    {
        const uint32_t anatomyCount = (uint32_t)medical.anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            MedicalSystem::HealthPartStatus* part = medical.anatomy[i];
            if (part == NULL) continue;

            std::map<const void*, float>::const_iterator prev = state.previousFlesh.find(part);
            if (prev == state.previousFlesh.end()) continue;

            const float maxHP = part->maxHealth();
            if (!RegenRules::IsUnexplainedFleshDrop(prev->second, part->flesh, maxHP)) continue;

            // DesolationSystem rewrites _maxHealth and clamps flesh down to it (ExternalModCompat.h).
            // A drop that stops exactly at a max now below the old flesh is that clamp, not a hit.
            if (IsExternalFleshClampTolerated() && FleshCapRules::IsFleshCapClamp(prev->second, part->flesh, maxHP)) continue;

            state.damageCooldown = g_config.woundHealingDelaySeconds;
            return;
        }
    }

    static void SnapshotFlesh(MedicalSystem& medical, CharacterRegenState& state)
    {
        const uint32_t anatomyCount = (uint32_t)medical.anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            MedicalSystem::HealthPartStatus* part = medical.anatomy[i];
            if (part != NULL) state.previousFlesh[part] = part->flesh;
        }
    }

    static void RegenerateBlood(MedicalSystem& medical, float dt)
    {
        float maxBlood = medical.getMaxBlood();
        if (maxBlood > 0.0f && medical.blood < maxBlood && g_config.bloodRegenerationSeconds > 0.0f)
        {
            float bloodRate = maxBlood / g_config.bloodRegenerationSeconds;
            medical.blood = ClampMax(medical.blood + (bloodRate * dt), maxBlood);
        }
    }

    static void HealBodyParts(Character* character, MedicalSystem& medical, float dt)
    {
        const bool toughnessFormula = (g_config.healingFormula == HEALING_FORMULA_TOUGHNESS);
        const float toughness = (character->stats != NULL) ? character->stats->toughness() : 0.0f;
        const float s = ToughnessScaling::Normalize(toughness, g_config.toughnessCap);

        PartHealInputs inputs;
        inputs.raceIsRobot = IsRobotRace(character);
        inputs.healSkeletonParts = g_config.healSkeletonParts;
        inputs.healRobotLimbsOnOrganics = g_config.healRobotLimbsOnOrganics;

        const uint32_t anatomyCount = (uint32_t)medical.anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            MedicalSystem::HealthPartStatus* part = medical.anatomy[i];
            if (part == NULL) continue;

            // Amputated stumps are only restored by limb regrowth.
            inputs.isStump = (part->getRobotLimbState() == LIMB_STUMP);
            inputs.isRobotic = part->isRobotic();
            if (!RegenRules::ShouldHealPart(inputs)) continue;

            const float maxHP = part->maxHealth();
            if (maxHP <= 0.0f) continue;

            // RegenStartBelowFraction: healthy parts do not regen, so high-level characters
            // are not topped up to full after every fight; one badly hurt limb still heals.
            // Effective health = flesh - fleshStun, the value the health bar shows (stun is the
            // darker, recoverable part of the bar). HealthPartStatus only exposes
            // derivedFleshHealthPercent as a derived field, whose exact formula is not verified,
            // so the ratio is computed from the raw fields against maxHealth().
            const float effectiveHealth = part->flesh - part->fleshStun;
            if (!RegenRules::ShouldStartPartRegen(effectiveHealth, maxHP, g_config.regenStartBelowFraction)) continue;

            float amount = 0.0f;
            float ceiling = maxHP;
            if (toughnessFormula)
            {
                const float baseMax = ResolveLimbBaseMax(part);
                amount = ToughnessScaling::RegenRatePerSecond(baseMax, s,
                    g_config.regenRateMinPercentPerMinute, g_config.regenRateMaxPercentPerMinute) * dt;
                ceiling = ClampMax(ToughnessScaling::RegenCeiling(baseMax, s,
                    g_config.regenCeilingMinFraction, g_config.regenCeilingMaxFraction), maxHP);
            }
            else
            {
                amount = RegenRules::LegacyRatePerSecond(maxHP, g_config.woundRegenerationSeconds, inputs.isRobotic) * dt;
            }

            if (g_config.enableFleshRegeneration)
            {
                part->flesh = RegenRules::HealFleshTowardsCeiling(part->flesh, amount, ceiling);
            }
            if (g_config.enableStunRegeneration)
            {
                part->fleshStun = RegenRules::RecoverDamageLayer(part->fleshStun, amount);
            }
            if (g_config.enableWearRegeneration)
            {
                part->wearDamage = RegenRules::RecoverDamageLayer(part->wearDamage, amount);
            }

            // Bandaging / Jury-rigging cleanup
            if (g_config.clearBleedingWhenHealed && part->flesh >= maxHP)
            {
                part->bandaging = 0.0f;
                part->juryRigging = 0.0f;
            }

            part->updateDerivedHealths();
        }

        // Refresh character's overall medical stats and posture (clears crippled state when legs/arms are positive)
        medical.updateStats();

        if (g_config.clearBleedingWhenHealed && medical.blood >= medical.getMaxBlood())
        {
            medical.currentBleedRate = 0.0f;
            medical.extraBloodLossFromBodyparts = 0.0f;
        }
    }

    static void ApplyDeathProtection(MedicalSystem& medical)
    {
        float minBlood = medical.getMaxBlood() * g_config.emergencyBloodFraction;
        if (medical.blood < minBlood)
        {
            medical.blood = minBlood;
        }

        uint32_t anatomyCount = (uint32_t)medical.anatomy.size();
        for (uint32_t i = 0; i < anatomyCount; ++i)
        {
            MedicalSystem::HealthPartStatus* part = medical.anatomy[i];
            if (part == NULL) continue;

            if (part->fatal && part->flesh < (part->maxHealth() * g_config.emergencyVitalFraction))
            {
                part->flesh = part->maxHealth() * g_config.emergencyVitalFraction;
                part->updateDerivedHealths();
            }
        }
    }

    static void ProcessCharacterHealing(Character* character, float dt)
    {
        if (character == NULL || character->isDead()) return;

        MedicalSystem& medical = character->medical;
        CharacterRegenState& state = g_regenStates.Get(character, IdentityOf(character));

        state.damageCooldown = RegenRules::TickCooldown(state.damageCooldown, dt);
        RunFleshWatchdog(medical, state);

        // Check if character is currently locked in combat (melee or ranged)
        bool inCombat = g_config.pauseHealingInCombat && character->isInCombatMode(true, true);
        if (inCombat)
        {
            state.damageCooldown = g_config.woundHealingDelaySeconds;
        }

        bool knockedOut = g_config.pauseHealingWhileUnconscious && IsKnockedOut(character);
        bool canRegenWounds = (state.damageCooldown <= 0.0f && !inCombat && !knockedOut);
        bool canRegenBlood = (!g_config.pauseBloodDuringDamage || state.damageCooldown <= 0.0f) && !inCombat && !knockedOut;

        // 1. Blood / Oil regeneration
        if (g_config.enableBloodRegeneration && canRegenBlood)
        {
            RegenerateBlood(medical, dt);
        }

        // 2. Body-part regeneration (Flesh, Stun, Wear)
        if (g_config.enableBodyPartRegeneration && canRegenWounds)
        {
            HealBodyParts(character, medical, dt);
        }

        // 3. Death protection floors (off by default: they made the squad immortal)
        if (g_config.enableDeathProtection)
        {
            ApplyDeathProtection(medical);
        }

        // 4. Limb regrowth for amputated stumps
        ProcessLimbRegrowth(character, dt, canRegenWounds);

        // 5. Watchdog snapshot after our own writes, so our healing is never read as damage
        SnapshotFlesh(medical, state);
    }

    static void GameWorld_mainLoop_hook(GameWorld* thisptr, float frameTime)
    {
        if (g_origMainLoop != NULL)
        {
            g_origMainLoop(thisptr, frameTime);
        }

        if (thisptr == NULL || thisptr->player == NULL)
        {
            return;
        }

        float dt = 0.0f;
        if (!RegenRules::SimulationDelta(frameTime, thisptr->getFrameSpeedMultiplier(), thisptr->isPaused(), dt))
        {
            return;
        }

        // Tick healing for all player characters
        const lektor<Character*>& characters = thisptr->player->getAllPlayerCharacters();
        const uint32_t characterCount = (uint32_t)characters.size();
        for (uint32_t i = 0; i < characterCount; ++i)
        {
            Character* c = characters[i];
            if (IsHealingEligible(c))
            {
                ProcessCharacterHealing(c, dt);
            }
        }
    }

    bool InitHealingEngine()
    {
        if (KenshiLib::SUCCESS != KenshiLib::AddHook(
                KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),
                GameWorld_mainLoop_hook,
                &g_origMainLoop))
        {
            LogError("Could not install GameWorld::mainLoop_GPUSensitiveStuff hook for HealingEngine.");
            return false;
        }

        InitDamageSignals();

        Log("HealingEngine hooks installed successfully.");
        return true;
    }
}
