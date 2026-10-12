#include "LimbMaxRules.h"

#include <string.h>

namespace TotalRestore
{
    LimbMaxSettings::LimbMaxSettings()
        : formula(LIMB_BONUS_SCALED)
        , toughnessCap(100.0f)
        , bonus()
        , healthPercentPerToughness(0.5f)
        , maxToughnessMultiplier(1.5f)
    {
    }

    namespace LimbMaxRules
    {
        // Fixed-point passes for the Scaled inverse. The bonus changes slowly with the
        // base (exponent <= 1 in practice), so a handful of passes converges well below 0.01 HP.
        static const int kInversePasses = 8;

        LimbBonusFormula ParseFormula(const char* text, LimbBonusFormula fallback)
        {
            if (text == NULL) return fallback;
            if (_stricmp(text, "Scaled") == 0) return LIMB_BONUS_SCALED;
            if (_stricmp(text, "Percent") == 0) return LIMB_BONUS_PERCENT;
            return fallback;
        }

        float ComputeLimbMax(float baseMax, float toughness, const LimbMaxSettings& settings)
        {
            if (!(baseMax > 0.0f)) return baseMax;

            if (settings.formula == LIMB_BONUS_PERCENT)
            {
                return baseMax * ToughnessScaling::PercentMultiplier(
                    toughness, settings.healthPercentPerToughness, settings.maxToughnessMultiplier);
            }

            const float s = ToughnessScaling::Normalize(toughness, settings.toughnessCap);
            return baseMax + ToughnessScaling::LimbBonus(baseMax, s, settings.bonus);
        }

        float EstimateBaseFromMax(float currentMax, float toughness, const LimbMaxSettings& settings)
        {
            if (!(currentMax > 0.0f)) return currentMax;

            if (settings.formula == LIMB_BONUS_PERCENT)
            {
                const float multiplier = ToughnessScaling::PercentMultiplier(
                    toughness, settings.healthPercentPerToughness, settings.maxToughnessMultiplier);
                return (multiplier > 0.0f) ? currentMax / multiplier : currentMax;
            }

            const float s = ToughnessScaling::Normalize(toughness, settings.toughnessCap);
            float base = currentMax;
            for (int pass = 0; pass < kInversePasses; ++pass)
            {
                const float candidate = currentMax - ToughnessScaling::LimbBonus(base, s, settings.bonus);
                if (!(candidate > 0.0f)) return currentMax;
                base = candidate;
            }
            return base;
        }

        bool ShouldScalePart(bool isRobotic, bool isAnimal, bool applyToRobots, bool applyToAnimals)
        {
            if (isRobotic && !applyToRobots) return false;
            if (isAnimal && !applyToAnimals) return false;
            return true;
        }

        float TargetLimbMax(bool scaled, float baseMax, float toughness, const LimbMaxSettings& settings)
        {
            return scaled ? ComputeLimbMax(baseMax, toughness, settings) : baseMax;
        }
    }
}
