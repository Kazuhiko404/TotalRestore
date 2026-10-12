#pragma once

#include "TotalRestore.h"
#include "LimbMaxRules.h"

/*
 * LimbToughness (game-facing)
 * Owns the per-(part, race) baseline cache and writes the toughness-scaled max
 * into HealthPartStatus::_maxHealth. Other modules only read through this API.
 */

namespace TotalRestore
{
    // Settings for the pure rules, built from g_config.
    LimbMaxSettings BuildLimbMaxSettings();

    // Unbuffed base max of a part: the captured baseline when known, otherwise the
    // current _maxHealth minus the bonus this module would have added.
    float ResolveLimbBaseMax(MedicalSystem::HealthPartStatus* part);

    // Re-applies the bonus to every part of a character (flesh proportion kept).
    void ApplyLimbToughness(MedicalSystem* medical);
}
