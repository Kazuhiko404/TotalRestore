#pragma once

/*
 * RegrowthRules
 * Pure rules of the HealingFactor limb regrowth port (no game includes, unit tested).
 * Limb indices follow RobotLimbs::Limb: 0 = LEFT_ARM, 1 = RIGHT_ARM, 2 = LEFT_LEG, 3 = RIGHT_LEG.
 * Raw limb states follow LimbState: 0 = ORIGINAL, 1 = STUMP, 2 = REPLACED, 3 = CRUSHED.
 */

namespace TotalRestore
{
    struct RegrowthSettings
    {
        bool enabled;
        bool limbEnabled[4];
        float regenerationSeconds;      // RegenerationSeconds (>= 1)
        float armRegenerationSeconds;   // 0 inherits
        float legRegenerationSeconds;   // 0 inherits
        float initialScale;             // 0.001 - 0.5
        float armInitialScale;          // 0 inherits, capped at 0.5
        float legInitialScale;          // 0 inherits, capped at 0.5
        float completionHealthFraction; // 0.01 - 1
        float armCompletionHealthFraction; // 0 inherits, capped at 1
        float legCompletionHealthFraction; // 0 inherits, capped at 1
        bool smoothCurve;               // LimbGrowthCurve: Smooth (default) or Linear

        RegrowthSettings();
    };

    enum SnapshotSource
    {
        SNAPSHOT_OWN,    // The limb's own snapshot, captured while it was intact
        SNAPSHOT_MIRROR, // Copy the opposite limb's snapshot
        SNAPSHOT_WAIT    // No snapshot anywhere: growth is deferred
    };

    enum RawStateVerdict
    {
        RAW_STATE_CONTINUE,       // State is what the regrowth expects
        RAW_STATE_STOP_REPLACED,  // A robotic limb was installed
        RAW_STATE_STOP_CHANGED    // Something else changed the limb
    };

    namespace RegrowthRules
    {
        bool IsArm(int limb);
        int MirrorLimb(int limb);

        // EnableLimbRegeneration && the per-limb switch.
        bool IsLimbEnabled(const RegrowthSettings& settings, int limb);
        bool AnyLimbEnabled(const RegrowthSettings& settings);

        float GrowthSeconds(const RegrowthSettings& settings, int limb);
        float InitialScale(const RegrowthSettings& settings, int limb);
        float CompletionFraction(const RegrowthSettings& settings, int limb);

        // "Linear" (case-insensitive) selects the linear curve; anything else is Smooth.
        bool ParseSmoothCurve(const char* text);

        // Bone scale for `elapsed` seconds of growth: initScale -> 1 (smoothstep or linear).
        float GrowthScale(float elapsed, float growthSeconds, float initScale, bool smooth);

        // Flesh given to a regrown limb: fraction * max, limited by the regen ceiling when
        // `useCeiling` (Toughness formula), never below 1 HP (HealingFactor floor).
        float CompletionFlesh(float fraction, float maxHealth, float ceiling, bool useCeiling);

        SnapshotSource ChooseSnapshotSource(bool ownSnapshotValid, bool mirrorSnapshotValid);

        // Expected raw state: ORIGINAL while the limb is in the shadow state, STUMP before the reveal.
        RawStateVerdict EvaluateRawState(int rawState, bool shadow);

        // The limb root keeps its captured positional size (it would detach from the
        // shoulder/hip); the other bones follow the growth scale.
        float PositionalSizeFactor(int boneIndex, float scale);

        // HealingFactor repair of a collapsed limb max: below 25% of the reference max.
        bool NeedsMaxRepair(float limbMax, float referenceMax);
    }
}
