#pragma once

/*
 * RegenRules
 * Pure per-tick rules of the healing engine (no game includes, unit tested).
 * The game-facing HealingEngine.cpp reads the engine state, calls these, and
 * writes the results back.
 */

namespace TotalRestore
{
    enum HealingFormula
    {
        HEALING_FORMULA_TOUGHNESS, // Toughness-scaled rate and ceiling on the unbuffed base (default)
        HEALING_FORMULA_LEGACY     // Old path: full max over WoundRegenerationSeconds, 0.5x for robotic parts
    };

    struct PartHealInputs
    {
        bool isStump;                  // Severed limb: only limb regrowth may restore it
        bool isRobotic;                // HealthPartStatus::isRobotic (skeleton body or prosthetic)
        bool raceIsRobot;              // RaceData::robot (skeleton races)
        bool healSkeletonParts;        // HealSkeletonParts
        bool healRobotLimbsOnOrganics; // HealRobotLimbsOnOrganics

        PartHealInputs();
    };

    namespace RegenRules
    {
        // Above this the game is fast-forwarding through a load or a bogus value; skip the tick.
        extern const float kMaxSpeedMultiplier;

        // Parses "Toughness" / "Legacy" (case-insensitive). Unknown text keeps the fallback.
        HealingFormula ParseFormula(const char* text, HealingFormula fallback);

        // Simulated seconds for this frame: frameDelta * speedMultiplier.
        // Returns false (no update) while paused or when the multiplier is < 0, > 100 or not finite.
        bool SimulationDelta(float frameDelta, float speedMultiplier, bool paused, float& outDelta);

        // HealingFactor watchdog: a flesh drop larger than max(0.5% of max, 0.5 HP) since
        // the previous tick is damage nobody reported through a hook.
        bool IsUnexplainedFleshDrop(float previousFlesh, float currentFlesh, float maxHealth);

        // Counts a cooldown down to 0.
        float TickCooldown(float cooldown, float delta);

        bool ShouldHealPart(const PartHealInputs& inputs);

        // RegenStartBelowFraction gate, per body part: passive regen only runs while
        // currentHealth / maxHealth is below startBelowFraction. A fraction >= 1 always heals
        // (old behaviour), a fraction <= 0 never heals, and a part with maxHealth <= 0 never heals.
        bool ShouldStartPartRegen(float currentHealth, float maxHealth, float startBelowFraction);

        // Clamps the configured RegenStartBelowFraction to [0, 1]; NaN becomes 1 (always heal).
        float ClampStartBelowFraction(float fraction);

        // Adds `amount` but never past the ceiling, and never lowers flesh already above it.
        float HealFleshTowardsCeiling(float flesh, float amount, float ceiling);

        // Recovers a damage layer (stun, wear) towards 0. Not limited by the ceiling.
        float RecoverDamageLayer(float damage, float amount);

        // Legacy HP per second: the full max over woundRegenerationSeconds, halved for robotic parts.
        float LegacyRatePerSecond(float maxHealth, float woundRegenerationSeconds, bool isRobotic);
    }
}
