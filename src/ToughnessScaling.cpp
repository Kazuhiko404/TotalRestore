#include "ToughnessScaling.h"

#include <float.h>
#include <math.h>

namespace TotalRestore
{
    namespace ToughnessScaling
    {
        static bool IsFinite(float value)
        {
            return _finite(value) != 0;
        }

        static float Clamp01(float value)
        {
            if (!IsFinite(value) || value < 0.0f) return 0.0f;
            return (value > 1.0f) ? 1.0f : value;
        }

        static bool IsValidMax(float value)
        {
            return IsFinite(value) && value > 0.0f;
        }

        LimbBonusParams::LimbBonusParams()
            : atReference(30.0f)
            , referenceHealth(100.0f)
            , exponent(0.25f)
            , maxPerPart(40.0f)
        {
        }

        float Normalize(float toughness, float cap)
        {
            if (!IsFinite(toughness) || !IsValidMax(cap)) return 0.0f;
            return Clamp01(toughness / cap);
        }

        float Lerp(float from, float to, float t)
        {
            const float clamped = Clamp01(t);
            return from + (to - from) * clamped;
        }

        float RegenCeiling(float baseMax, float s, float minFraction, float maxFraction)
        {
            if (!IsValidMax(baseMax)) return 0.0f;
            const float fraction = Lerp(minFraction, maxFraction, s);
            return (fraction > 0.0f) ? baseMax * fraction : 0.0f;
        }

        float RegenRatePerSecond(float baseMax, float s, float minPercentPerMinute, float maxPercentPerMinute)
        {
            if (!IsValidMax(baseMax)) return 0.0f;
            const float percentPerMinute = Lerp(minPercentPerMinute, maxPercentPerMinute, s);
            if (percentPerMinute <= 0.0f) return 0.0f;
            return baseMax * percentPerMinute / 100.0f / 60.0f;
        }

        float LimbBonus(float baseMax, float s, const LimbBonusParams& params)
        {
            if (!IsValidMax(baseMax) || !IsFinite(s)) return 0.0f;

            float raceFactor = 1.0f;
            if (IsValidMax(params.referenceHealth) && IsFinite(params.exponent) && params.exponent != 0.0f)
            {
                raceFactor = powf(params.referenceHealth / baseMax, params.exponent);
            }

            float bonus = Clamp01(s) * params.atReference * raceFactor;
            if (!IsFinite(bonus) || bonus < 0.0f) return 0.0f;
            if (params.maxPerPart > 0.0f && bonus > params.maxPerPart)
            {
                bonus = params.maxPerPart;
            }
            return bonus;
        }

        float PercentMultiplier(float toughness, float percentPerPoint, float maxMultiplier)
        {
            if (!IsFinite(toughness) || !IsFinite(percentPerPoint)) return 1.0f;

            float multiplier = 1.0f + (toughness * (percentPerPoint / 100.0f));
            if (maxMultiplier > 1.0f && multiplier > maxMultiplier)
            {
                multiplier = maxMultiplier;
            }
            return multiplier;
        }

        float RescaleFlesh(float hp, float oldMax, float newMax)
        {
            if (!IsValidMax(oldMax) || !IsValidMax(newMax)) return hp;
            return hp * (newMax / oldMax);
        }
    }
}
