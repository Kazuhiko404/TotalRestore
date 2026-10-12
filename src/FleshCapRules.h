#pragma once

/*
 * FleshCapRules
 * Pure rule for the flesh-drop watchdog when another mod owns the max HP
 * (no game includes, unit tested).
 *
 * DesolationSystem rewrites HealthPartStatus::_maxHealth from its own baseline
 * after every MedicalSystem::updateStats and then clamps flesh down to
 * age * HPMult * max - wearDamage (its "D clamp-back"). TotalRestore writes
 * _maxHealth too, so a part's max can shrink under flesh and the clamp follows.
 * That drop is not damage and must not restart WoundHealingDelaySeconds.
 */

namespace TotalRestore
{
    namespace FleshCapRules
    {
        // True when flesh was above the max that now applies (previousFlesh > maxHealth)
        // and the drop stops at that max (within float rounding): a clamp, not a hit.
        // A drop that lands below the max is still damage. Invalid input: false.
        bool IsFleshCapClamp(float previousFlesh, float currentFlesh, float maxHealth);
    }
}
