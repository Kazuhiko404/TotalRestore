// Cases mirror the approved formulas in odd/tasks/balance-overhaul.md.

#include "ToughnessScaling.h"
#include "test_check.h"
#include "test_suites.h"

#include <limits>

using namespace TotalRestore::ToughnessScaling;

namespace
{
    float NotANumber()
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    float Infinity()
    {
        return std::numeric_limits<float>::infinity();
    }

    LimbBonusParams DefaultBonus()
    {
        LimbBonusParams p;
        p.atReference = 30.0f;
        p.referenceHealth = 100.0f;
        p.exponent = 0.25f;
        p.maxPerPart = 40.0f;
        return p;
    }

    void NormalizeClampsToughnessToTheCap()
    {
        const char* name = "NormalizeClampsToughnessToTheCap";
        CHECK_NEAR(Normalize(0.0f, 100.0f), 0.0f, 1e-6f);
        CHECK_NEAR(Normalize(50.0f, 100.0f), 0.5f, 1e-6f);
        CHECK_NEAR(Normalize(100.0f, 100.0f), 1.0f, 1e-6f);
        CHECK_NEAR(Normalize(106.0f, 100.0f), 1.0f, 1e-6f);
        CHECK_NEAR(Normalize(250.0f, 100.0f), 1.0f, 1e-6f);
        CHECK_NEAR(Normalize(-5.0f, 100.0f), 0.0f, 1e-6f);
        CHECK_NEAR(Normalize(40.0f, 80.0f), 0.5f, 1e-6f);
    }

    void NormalizeRejectsInvalidInput()
    {
        const char* name = "NormalizeRejectsInvalidInput";
        CHECK_NEAR(Normalize(NotANumber(), 100.0f), 0.0f, 1e-6f);
        CHECK_NEAR(Normalize(Infinity(), 100.0f), 0.0f, 1e-6f);
        CHECK_NEAR(Normalize(50.0f, 0.0f), 0.0f, 1e-6f);
        CHECK_NEAR(Normalize(50.0f, -10.0f), 0.0f, 1e-6f);
        CHECK_NEAR(Normalize(50.0f, NotANumber()), 0.0f, 1e-6f);
    }

    void LerpClampsT()
    {
        const char* name = "LerpClampsT";
        CHECK_NEAR(Lerp(0.10f, 0.50f, 0.0f), 0.10f, 1e-6f);
        CHECK_NEAR(Lerp(0.10f, 0.50f, 0.5f), 0.30f, 1e-6f);
        CHECK_NEAR(Lerp(0.10f, 0.50f, 1.0f), 0.50f, 1e-6f);
        CHECK_NEAR(Lerp(0.10f, 0.50f, 2.0f), 0.50f, 1e-6f);
        CHECK_NEAR(Lerp(0.10f, 0.50f, -1.0f), 0.10f, 1e-6f);
    }

    void RegenCeilingFollowsToughness()
    {
        const char* name = "RegenCeilingFollowsToughness";
        CHECK_NEAR(RegenCeiling(100.0f, 0.0f, 0.10f, 0.50f), 10.0f, 1e-4f);
        CHECK_NEAR(RegenCeiling(100.0f, 0.5f, 0.10f, 0.50f), 30.0f, 1e-4f);
        CHECK_NEAR(RegenCeiling(100.0f, 1.0f, 0.10f, 0.50f), 50.0f, 1e-4f);
        CHECK_NEAR(RegenCeiling(220.0f, 1.0f, 0.10f, 0.50f), 110.0f, 1e-4f);
        CHECK_NEAR(RegenCeiling(200.0f, Normalize(106.0f, 100.0f), 0.10f, 0.50f), 100.0f, 1e-4f);
        // Cheat mode: a full refill is still expressible.
        CHECK_NEAR(RegenCeiling(100.0f, 1.0f, 1.0f, 1.0f), 100.0f, 1e-4f);
    }

    void RegenCeilingRejectsInvalidBase()
    {
        const char* name = "RegenCeilingRejectsInvalidBase";
        CHECK_NEAR(RegenCeiling(0.0f, 1.0f, 0.10f, 0.50f), 0.0f, 1e-6f);
        CHECK_NEAR(RegenCeiling(-50.0f, 1.0f, 0.10f, 0.50f), 0.0f, 1e-6f);
        CHECK_NEAR(RegenCeiling(NotANumber(), 1.0f, 0.10f, 0.50f), 0.0f, 1e-6f);
    }

    void RegenRateIsPercentOfBasePerMinute()
    {
        const char* name = "RegenRateIsPercentOfBasePerMinute";
        // 1 %/min of 100 HP = 1 HP per 60 s.
        CHECK_NEAR(RegenRatePerSecond(100.0f, 0.0f, 1.0f, 4.0f), 1.0f / 60.0f, 1e-6f);
        // 4 %/min of 100 HP = 4 HP per 60 s.
        CHECK_NEAR(RegenRatePerSecond(100.0f, 1.0f, 1.0f, 4.0f), 4.0f / 60.0f, 1e-6f);
        // 2.5 %/min of 200 HP = 5 HP per 60 s.
        CHECK_NEAR(RegenRatePerSecond(200.0f, 0.5f, 1.0f, 4.0f), 5.0f / 60.0f, 1e-6f);
        // T106 is capped: same speed as T100.
        CHECK_NEAR(RegenRatePerSecond(100.0f, Normalize(106.0f, 100.0f), 1.0f, 4.0f), 4.0f / 60.0f, 1e-6f);
        CHECK_NEAR(RegenRatePerSecond(0.0f, 1.0f, 1.0f, 4.0f), 0.0f, 1e-6f);
        CHECK_NEAR(RegenRatePerSecond(NotANumber(), 1.0f, 1.0f, 4.0f), 0.0f, 1e-6f);
    }

    void LimbBonusMatchesApprovedTable()
    {
        const char* name = "LimbBonusMatchesApprovedTable";
        const LimbBonusParams p = DefaultBonus();
        CHECK_NEAR(LimbBonus(80.0f, 1.0f, p), 31.7f, 0.1f);
        CHECK_NEAR(LimbBonus(100.0f, 1.0f, p), 30.0f, 0.1f);
        CHECK_NEAR(LimbBonus(200.0f, 1.0f, p), 25.2f, 0.1f);
        CHECK_NEAR(LimbBonus(220.0f, 1.0f, p), 24.6f, 0.1f);
    }

    void LimbBonusIsInvertedAndScalesWithToughness()
    {
        const char* name = "LimbBonusIsInvertedAndScalesWithToughness";
        const LimbBonusParams p = DefaultBonus();
        CHECK(LimbBonus(80.0f, 1.0f, p) > LimbBonus(220.0f, 1.0f, p));
        CHECK_NEAR(LimbBonus(100.0f, 0.5f, p), 15.0f, 0.01f);
        CHECK_NEAR(LimbBonus(100.0f, 0.0f, p), 0.0f, 1e-6f);
        // s is clamped: an over-range s cannot exceed the s = 1 bonus.
        CHECK_NEAR(LimbBonus(100.0f, 3.0f, p), 30.0f, 0.01f);
    }

    void LimbBonusAcceptanceAtT106Base220()
    {
        const char* name = "LimbBonusAcceptanceAtT106Base220";
        const LimbBonusParams p = DefaultBonus();
        const float newMax = 220.0f + LimbBonus(220.0f, Normalize(106.0f, 100.0f), p);
        CHECK_NEAR(newMax, 244.6f, 0.1f);
    }

    void LimbBonusIsClampedPerPart()
    {
        const char* name = "LimbBonusIsClampedPerPart";
        LimbBonusParams p = DefaultBonus();
        // Tiny base would explode without the clamp: 30 * (100 / 1) ^ 0.25 = 94.9.
        CHECK_NEAR(LimbBonus(1.0f, 1.0f, p), 40.0f, 1e-4f);
        p.maxPerPart = 0.0f; // <= 0 means uncapped
        CHECK_NEAR(LimbBonus(1.0f, 1.0f, p), 94.87f, 0.05f);
    }

    void LimbBonusHandlesEdgeCases()
    {
        const char* name = "LimbBonusHandlesEdgeCases";
        LimbBonusParams p = DefaultBonus();
        CHECK_NEAR(LimbBonus(0.0f, 1.0f, p), 0.0f, 1e-6f);
        CHECK_NEAR(LimbBonus(-10.0f, 1.0f, p), 0.0f, 1e-6f);
        CHECK_NEAR(LimbBonus(NotANumber(), 1.0f, p), 0.0f, 1e-6f);
        CHECK_NEAR(LimbBonus(Infinity(), 1.0f, p), 0.0f, 1e-6f);
        CHECK_NEAR(LimbBonus(100.0f, NotANumber(), p), 0.0f, 1e-6f);

        p.exponent = 0.0f; // flat bonus for every base
        CHECK_NEAR(LimbBonus(80.0f, 1.0f, p), 30.0f, 1e-4f);
        CHECK_NEAR(LimbBonus(220.0f, 1.0f, p), 30.0f, 1e-4f);

        p = DefaultBonus();
        p.referenceHealth = 0.0f; // no reference: flat bonus
        CHECK_NEAR(LimbBonus(220.0f, 1.0f, p), 30.0f, 1e-4f);

        p = DefaultBonus();
        p.atReference = -5.0f; // never negative
        CHECK_NEAR(LimbBonus(100.0f, 1.0f, p), 0.0f, 1e-6f);
    }

    void PercentMultiplierKeepsLegacyBehaviour()
    {
        const char* name = "PercentMultiplierKeepsLegacyBehaviour";
        CHECK_NEAR(PercentMultiplier(50.0f, 0.5f, 1.5f), 1.25f, 1e-5f);
        CHECK_NEAR(PercentMultiplier(106.0f, 0.5f, 1.5f), 1.5f, 1e-5f);
        // A cap <= 1 means "no cap", as in the original module.
        CHECK_NEAR(PercentMultiplier(106.0f, 0.5f, 1.0f), 1.53f, 1e-5f);
        CHECK_NEAR(PercentMultiplier(0.0f, 0.5f, 1.5f), 1.0f, 1e-6f);
        CHECK_NEAR(PercentMultiplier(NotANumber(), 0.5f, 1.5f), 1.0f, 1e-6f);
    }

    void RescaleFleshKeepsProportionWithoutFlooring()
    {
        const char* name = "RescaleFleshKeepsProportionWithoutFlooring";
        CHECK_NEAR(RescaleFlesh(50.0f, 100.0f, 130.0f), 65.0f, 1e-4f);
        CHECK_NEAR(RescaleFlesh(33.3f, 100.0f, 130.0f), 43.29f, 1e-3f);
        // Fractions survive (the original floorf would have produced 0).
        CHECK_NEAR(RescaleFlesh(0.5f, 100.0f, 130.0f), 0.65f, 1e-5f);
        // Negative flesh (a dying part) keeps its proportion too.
        CHECK_NEAR(RescaleFlesh(-20.0f, 100.0f, 150.0f), -30.0f, 1e-4f);
        CHECK_NEAR(RescaleFlesh(130.0f, 130.0f, 100.0f), 100.0f, 1e-4f);
    }

    void RescaleFleshRejectsInvalidMax()
    {
        const char* name = "RescaleFleshRejectsInvalidMax";
        CHECK_NEAR(RescaleFlesh(50.0f, 0.0f, 130.0f), 50.0f, 1e-6f);
        CHECK_NEAR(RescaleFlesh(50.0f, -1.0f, 130.0f), 50.0f, 1e-6f);
        CHECK_NEAR(RescaleFlesh(50.0f, 100.0f, NotANumber()), 50.0f, 1e-6f);
        CHECK_NEAR(RescaleFlesh(50.0f, 100.0f, 0.0f), 50.0f, 1e-6f);
    }
}

void RunToughnessScalingTests()
{
    NormalizeClampsToughnessToTheCap();
    NormalizeRejectsInvalidInput();
    LerpClampsT();
    RegenCeilingFollowsToughness();
    RegenCeilingRejectsInvalidBase();
    RegenRateIsPercentOfBasePerMinute();
    LimbBonusMatchesApprovedTable();
    LimbBonusIsInvertedAndScalesWithToughness();
    LimbBonusAcceptanceAtT106Base220();
    LimbBonusIsClampedPerPart();
    LimbBonusHandlesEdgeCases();
    PercentMultiplierKeepsLegacyBehaviour();
    RescaleFleshKeepsProportionWithoutFlooring();
    RescaleFleshRejectsInvalidMax();
}
