// Limb regrowth rules (T5): per-limb settings, growth curve, completion HP,
// snapshot source choice, raw-state checks.

#include "RegrowthRules.h"
#include "test_check.h"
#include "test_suites.h"

using namespace TotalRestore;

namespace
{
    void DefaultsMatchTheIni()
    {
        const char* name = "RegrowthDefaultsMatchTheIni";
        const RegrowthSettings s;
        CHECK(s.enabled);
        CHECK(s.limbEnabled[0] && s.limbEnabled[1] && s.limbEnabled[2] && s.limbEnabled[3]);
        CHECK_NEAR(s.regenerationSeconds, 120.0f, 1e-6f);
        CHECK_NEAR(s.initialScale, 0.03f, 1e-6f);
        CHECK_NEAR(s.completionHealthFraction, 0.10f, 1e-6f);
        CHECK(s.smoothCurve);
    }

    void LimbTopology()
    {
        const char* name = "LimbTopology";
        CHECK(RegrowthRules::IsArm(0) && RegrowthRules::IsArm(1));
        CHECK(!RegrowthRules::IsArm(2) && !RegrowthRules::IsArm(3));
        CHECK(RegrowthRules::MirrorLimb(0) == 1);
        CHECK(RegrowthRules::MirrorLimb(1) == 0);
        CHECK(RegrowthRules::MirrorLimb(2) == 3);
        CHECK(RegrowthRules::MirrorLimb(3) == 2);
        CHECK(RegrowthRules::MirrorLimb(7) == -1);
    }

    void LimbSwitches()
    {
        const char* name = "LimbSwitches";
        RegrowthSettings s;
        CHECK(RegrowthRules::IsLimbEnabled(s, 2));
        s.limbEnabled[2] = false;
        CHECK(!RegrowthRules::IsLimbEnabled(s, 2));
        CHECK(RegrowthRules::AnyLimbEnabled(s));
        CHECK(!RegrowthRules::IsLimbEnabled(s, 4));
        CHECK(!RegrowthRules::IsLimbEnabled(s, -1));
        s.enabled = false;
        CHECK(!RegrowthRules::IsLimbEnabled(s, 0));
        CHECK(!RegrowthRules::AnyLimbEnabled(s));
        s.enabled = true;
        s.limbEnabled[0] = s.limbEnabled[1] = s.limbEnabled[3] = false;
        CHECK(!RegrowthRules::AnyLimbEnabled(s));
    }

    void PerLimbOverridesInheritAndClamp()
    {
        const char* name = "PerLimbOverridesInheritAndClamp";
        RegrowthSettings s;
        CHECK_NEAR(RegrowthRules::GrowthSeconds(s, 0), 120.0f, 1e-6f);
        s.armRegenerationSeconds = 60.0f;
        CHECK_NEAR(RegrowthRules::GrowthSeconds(s, 1), 60.0f, 1e-6f);
        CHECK_NEAR(RegrowthRules::GrowthSeconds(s, 3), 120.0f, 1e-6f);
        s.regenerationSeconds = 0.0f; // clamped to >= 1
        CHECK_NEAR(RegrowthRules::GrowthSeconds(s, 2), 1.0f, 1e-6f);

        CHECK_NEAR(RegrowthRules::InitialScale(s, 0), 0.03f, 1e-6f);
        s.legInitialScale = 0.9f; // capped at 0.5
        CHECK_NEAR(RegrowthRules::InitialScale(s, 2), 0.5f, 1e-6f);
        s.initialScale = 0.0f;    // clamped to 0.001
        CHECK_NEAR(RegrowthRules::InitialScale(s, 0), 0.001f, 1e-6f);

        CHECK_NEAR(RegrowthRules::CompletionFraction(s, 0), 0.10f, 1e-6f);
        s.armCompletionHealthFraction = 2.0f; // capped at 1
        CHECK_NEAR(RegrowthRules::CompletionFraction(s, 0), 1.0f, 1e-6f);
        s.completionHealthFraction = 0.0f;    // clamped to 0.01
        CHECK_NEAR(RegrowthRules::CompletionFraction(s, 3), 0.01f, 1e-6f);
    }

    void CurveParsing()
    {
        const char* name = "CurveParsing";
        CHECK(RegrowthRules::ParseSmoothCurve("Smooth"));
        CHECK(!RegrowthRules::ParseSmoothCurve("Linear"));
        CHECK(!RegrowthRules::ParseSmoothCurve("linear"));
        CHECK(RegrowthRules::ParseSmoothCurve("whatever"));
        CHECK(RegrowthRules::ParseSmoothCurve(0));
    }

    void GrowthScaleFollowsTheCurve()
    {
        const char* name = "GrowthScaleFollowsTheCurve";
        CHECK_NEAR(RegrowthRules::GrowthScale(0.0f, 120.0f, 0.03f, true), 0.03f, 1e-6f);
        CHECK_NEAR(RegrowthRules::GrowthScale(120.0f, 120.0f, 0.03f, true), 1.0f, 1e-6f);
        CHECK_NEAR(RegrowthRules::GrowthScale(500.0f, 120.0f, 0.03f, true), 1.0f, 1e-6f);
        // Midpoint: smoothstep(0.5) = 0.5 and linear 0.5.
        CHECK_NEAR(RegrowthRules::GrowthScale(60.0f, 120.0f, 0.0f, true), 0.5f, 1e-6f);
        // Quarter: smoothstep(0.25) = 0.15625, linear 0.25.
        CHECK_NEAR(RegrowthRules::GrowthScale(30.0f, 120.0f, 0.0f, true), 0.15625f, 1e-6f);
        CHECK_NEAR(RegrowthRules::GrowthScale(30.0f, 120.0f, 0.0f, false), 0.25f, 1e-6f);
        CHECK_NEAR(RegrowthRules::GrowthScale(30.0f, 120.0f, 0.2f, false), 0.4f, 1e-6f);
        CHECK_NEAR(RegrowthRules::GrowthScale(-5.0f, 120.0f, 0.03f, false), 0.03f, 1e-6f);
        CHECK_NEAR(RegrowthRules::GrowthScale(10.0f, 0.0f, 0.03f, false), 1.0f, 1e-6f);
    }

    void CompletionFleshRespectsTheCeiling()
    {
        const char* name = "CompletionFleshRespectsTheCeiling";
        // Toughness formula: min(fraction * max, ceiling).
        CHECK_NEAR(RegrowthRules::CompletionFlesh(0.10f, 130.0f, 50.0f, true), 13.0f, 1e-5f);
        CHECK_NEAR(RegrowthRules::CompletionFlesh(0.80f, 130.0f, 50.0f, true), 50.0f, 1e-5f);
        // Legacy formula: fraction only.
        CHECK_NEAR(RegrowthRules::CompletionFlesh(0.80f, 130.0f, 50.0f, false), 104.0f, 1e-4f);
        // HealingFactor floor: never below 1 HP.
        CHECK_NEAR(RegrowthRules::CompletionFlesh(0.01f, 50.0f, 50.0f, true), 1.0f, 1e-6f);
        CHECK_NEAR(RegrowthRules::CompletionFlesh(0.10f, 130.0f, 0.0f, true), 1.0f, 1e-6f);
        CHECK_NEAR(RegrowthRules::CompletionFlesh(0.10f, 0.0f, 50.0f, false), 1.0f, 1e-6f);
    }

    void SnapshotSourcePrefersOwnThenMirror()
    {
        const char* name = "SnapshotSourcePrefersOwnThenMirror";
        CHECK(RegrowthRules::ChooseSnapshotSource(true, true) == SNAPSHOT_OWN);
        CHECK(RegrowthRules::ChooseSnapshotSource(true, false) == SNAPSHOT_OWN);
        CHECK(RegrowthRules::ChooseSnapshotSource(false, true) == SNAPSHOT_MIRROR);
        CHECK(RegrowthRules::ChooseSnapshotSource(false, false) == SNAPSHOT_WAIT);
    }

    void RawStateChecks()
    {
        const char* name = "RawStateChecks";
        // Before the reveal the raw state is STUMP; after it, ORIGINAL (shadow).
        CHECK(RegrowthRules::EvaluateRawState(1, false) == RAW_STATE_CONTINUE);
        CHECK(RegrowthRules::EvaluateRawState(0, true) == RAW_STATE_CONTINUE);
        CHECK(RegrowthRules::EvaluateRawState(2, false) == RAW_STATE_STOP_REPLACED);
        CHECK(RegrowthRules::EvaluateRawState(2, true) == RAW_STATE_STOP_REPLACED);
        CHECK(RegrowthRules::EvaluateRawState(0, false) == RAW_STATE_STOP_CHANGED);
        CHECK(RegrowthRules::EvaluateRawState(1, true) == RAW_STATE_STOP_CHANGED);
        CHECK(RegrowthRules::EvaluateRawState(3, false) == RAW_STATE_STOP_CHANGED);
    }

    void RootBoneKeepsItsPositionalSize()
    {
        const char* name = "RootBoneKeepsItsPositionalSize";
        CHECK_NEAR(RegrowthRules::PositionalSizeFactor(0, 0.3f), 1.0f, 1e-6f);
        CHECK_NEAR(RegrowthRules::PositionalSizeFactor(1, 0.3f), 0.3f, 1e-6f);
        CHECK_NEAR(RegrowthRules::PositionalSizeFactor(3, 0.3f), 0.3f, 1e-6f);
    }

    void CollapsedMaxRepair()
    {
        const char* name = "CollapsedMaxRepair";
        CHECK(RegrowthRules::NeedsMaxRepair(10.0f, 100.0f));
        CHECK(!RegrowthRules::NeedsMaxRepair(25.0f, 100.0f));
        CHECK(!RegrowthRules::NeedsMaxRepair(80.0f, 100.0f));
        CHECK(!RegrowthRules::NeedsMaxRepair(0.0f, 100.0f));
        CHECK(!RegrowthRules::NeedsMaxRepair(10.0f, 0.0f));
    }
}

void RunRegrowthRulesTests()
{
    DefaultsMatchTheIni();
    LimbTopology();
    LimbSwitches();
    PerLimbOverridesInheritAndClamp();
    CurveParsing();
    GrowthScaleFollowsTheCurve();
    CompletionFleshRespectsTheCeiling();
    SnapshotSourcePrefersOwnThenMirror();
    RawStateChecks();
    RootBoneKeepsItsPositionalSize();
    CollapsedMaxRepair();
}
