#include "RegenRules.h"

#include <float.h>
#include <string.h>

namespace TotalRestore
{
    PartHealInputs::PartHealInputs()
        : isStump(false)
        , isRobotic(false)
        , raceIsRobot(false)
        , healSkeletonParts(true)
        , healRobotLimbsOnOrganics(false)
    {
    }

    namespace RegenRules
    {
        const float kMaxSpeedMultiplier = 100.0f;

        // HealingFactor watchdog constants: a drop must exceed 0.5% of max, and at least 0.5 HP.
        static const float kWatchdogMaxFraction = 0.005f;
        static const float kWatchdogMinDrop = 0.5f;

        // The legacy path divided by WoundRegenerationSeconds; 0 meant "instant". Keep it instant but finite.
        static const float kInstantSeconds = 0.001f;

        static bool IsFinite(float value)
        {
            return _finite(value) != 0;
        }

        HealingFormula ParseFormula(const char* text, HealingFormula fallback)
        {
            if (text == NULL) return fallback;
            if (_stricmp(text, "Toughness") == 0) return HEALING_FORMULA_TOUGHNESS;
            if (_stricmp(text, "Legacy") == 0) return HEALING_FORMULA_LEGACY;
            return fallback;
        }

        bool SimulationDelta(float frameDelta, float speedMultiplier, bool paused, float& outDelta)
        {
            outDelta = 0.0f;
            if (paused) return false;
            if (!IsFinite(frameDelta) || frameDelta < 0.0f) return false;
            if (!IsFinite(speedMultiplier) || speedMultiplier < 0.0f || speedMultiplier > kMaxSpeedMultiplier) return false;

            outDelta = frameDelta * speedMultiplier;
            return true;
        }

        bool IsUnexplainedFleshDrop(float previousFlesh, float currentFlesh, float maxHealth)
        {
            float threshold = kWatchdogMaxFraction * maxHealth;
            if (!(threshold > kWatchdogMinDrop)) threshold = kWatchdogMinDrop;
            return (previousFlesh - threshold) > currentFlesh;
        }

        float TickCooldown(float cooldown, float delta)
        {
            const float remaining = cooldown - delta;
            return (remaining > 0.0f) ? remaining : 0.0f;
        }

        bool ShouldStartPartRegen(float currentHealth, float maxHealth, float startBelowFraction)
        {
            if (!(maxHealth > 0.0f)) return false;
            if (startBelowFraction >= 1.0f) return true;
            if (!(startBelowFraction > 0.0f)) return false; // also rejects NaN
            return (currentHealth / maxHealth) < startBelowFraction;
        }

        float ClampStartBelowFraction(float fraction)
        {
            if (fraction != fraction) return 1.0f;
            if (fraction < 0.0f) return 0.0f;
            return (fraction > 1.0f) ? 1.0f : fraction;
        }

        bool ShouldHealPart(const PartHealInputs& inputs)
        {
            if (inputs.isStump) return false;
            if (!inputs.isRobotic) return true;
            return inputs.raceIsRobot ? inputs.healSkeletonParts : inputs.healRobotLimbsOnOrganics;
        }

        float HealFleshTowardsCeiling(float flesh, float amount, float ceiling)
        {
            if (flesh >= ceiling || !(amount > 0.0f)) return flesh;
            const float healed = flesh + amount;
            return (healed > ceiling) ? ceiling : healed;
        }

        float RecoverDamageLayer(float damage, float amount)
        {
            if (!(damage > 0.0f)) return damage;
            const float remaining = damage - amount;
            return (remaining > 0.0f) ? remaining : 0.0f;
        }

        float LegacyRatePerSecond(float maxHealth, float woundRegenerationSeconds, bool isRobotic)
        {
            if (!(maxHealth > 0.0f)) return 0.0f;

            const float seconds = (woundRegenerationSeconds > 0.0f) ? woundRegenerationSeconds : kInstantSeconds;
            float rate = maxHealth / seconds;

            // Skeleton parts have about twice the human base HP; the old path halved their speed.
            if (isRobotic) rate *= 0.5f;
            return rate;
        }
    }
}
