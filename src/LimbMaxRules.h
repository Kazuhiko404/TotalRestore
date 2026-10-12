#pragma once

#include "ToughnessScaling.h"

/*
 * LimbMaxRules
 * Pure rules for the limb max-HP bonus (no game includes, unit tested).
 * The game-facing module (LimbToughness.cpp) writes the result into
 * HealthPartStatus::_maxHealth, because the native validateHealthValues
 * clamps flesh against that raw field, not against the maxHealth() getter.
 */

namespace TotalRestore
{
    enum LimbBonusFormula
    {
        LIMB_BONUS_SCALED,  // _maxHealth = base + LimbBonus(base, s)   (default)
        LIMB_BONUS_PERCENT  // _maxHealth = base * PercentMultiplier(T) (legacy LimbToughness behaviour)
    };

    struct LimbMaxSettings
    {
        LimbBonusFormula formula;
        float toughnessCap;
        ToughnessScaling::LimbBonusParams bonus;
        float healthPercentPerToughness; // Percent formula only
        float maxToughnessMultiplier;    // Percent formula only

        LimbMaxSettings();
    };

    namespace LimbMaxRules
    {
        // Parses "Scaled" / "Percent" (case-insensitive). Unknown text keeps the fallback.
        LimbBonusFormula ParseFormula(const char* text, LimbBonusFormula fallback);

        // The max HP a part with this unbuffed base should have at this Toughness.
        float ComputeLimbMax(float baseMax, float toughness, const LimbMaxSettings& settings);

        // Inverse of ComputeLimbMax, used when no baseline was ever captured for a part.
        float EstimateBaseFromMax(float currentMax, float toughness, const LimbMaxSettings& settings);

        // Robots/skeleton parts and animals are only buffed when their switch is on.
        bool ShouldScalePart(bool isRobotic, bool isAnimal, bool applyToRobots, bool applyToAnimals);

        // The max HP a part should end up with: the captured base when it is not scaled
        // (repairs a bonus baked into an older save), ComputeLimbMax otherwise.
        float TargetLimbMax(bool scaled, float baseMax, float toughness, const LimbMaxSettings& settings);
    }
}
