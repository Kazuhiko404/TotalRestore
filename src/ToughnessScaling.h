#pragma once

/*
 * ToughnessScaling
 * Pure balance math shared by the limb max-HP bonus and the healing engine.
 * No game includes: this module is compiled into the unit test project as-is.
 *
 * Approved formulas (odd/tasks/balance-overhaul.md):
 *   s       = clamp(T, 0, cap) / cap
 *   ceiling = baseMax * lerp(minFraction, maxFraction, s)
 *   HP/s    = baseMax * lerp(minPctPerMin, maxPctPerMin, s) / 100 / 60
 *   bonus   = s * atReference * (referenceHealth / baseMax) ^ exponent, clamped to [0, maxPerPart]
 */

namespace TotalRestore
{
    namespace ToughnessScaling
    {
        struct LimbBonusParams
        {
            float atReference;     // Bonus HP at s = 1 for a part whose base equals referenceHealth
            float referenceHealth; // Base max HP that receives exactly atReference
            float exponent;        // 0 = same bonus for every base; > 0 = low-HP parts get more
            float maxPerPart;      // Upper clamp; <= 0 means uncapped

            LimbBonusParams();
        };

        // Toughness normalized to [0, 1]. Non-finite input or a non-positive cap yields 0.
        float Normalize(float toughness, float cap);

        // Linear interpolation with t clamped to [0, 1].
        float Lerp(float from, float to, float t);

        // Highest flesh value passive regeneration may refill to.
        float RegenCeiling(float baseMax, float s, float minFraction, float maxFraction);

        // Passive regeneration speed in HP per simulated second.
        float RegenRatePerSecond(float baseMax, float s, float minPercentPerMinute, float maxPercentPerMinute);

        // Inverted, capped limb max-HP bonus. Returns 0 for invalid input.
        float LimbBonus(float baseMax, float s, const LimbBonusParams& params);

        // Legacy "Percent" multiplier: 1 + T * percentPerPoint / 100, capped by maxMultiplier when it is > 1.
        float PercentMultiplier(float toughness, float percentPerPoint, float maxMultiplier);

        // Keeps the flesh proportion when the max changes. Never floors (keeps fractions).
        float RescaleFlesh(float hp, float oldMax, float newMax);
    }
}
