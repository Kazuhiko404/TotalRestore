// Healing engine rules (T3): sim delta, watchdog, part eligibility, ceiling,
// legacy rate, and the per-character state registry.

#include "RegenRules.h"
#include "RegenStateRegistry.h"
#include "test_check.h"
#include "test_suites.h"

#include <limits>

using namespace TotalRestore;

namespace
{
    PartHealInputs Organic()
    {
        PartHealInputs in;
        in.isStump = false;
        in.isRobotic = false;
        in.raceIsRobot = false;
        in.healSkeletonParts = true;
        in.healRobotLimbsOnOrganics = false;
        return in;
    }

    void DefaultPartInputsAreAnOrganicPart()
    {
        const char* name = "DefaultPartInputsAreAnOrganicPart";
        const PartHealInputs in;
        CHECK(!in.isStump);
        CHECK(!in.isRobotic);
        CHECK(!in.raceIsRobot);
        CHECK(in.healSkeletonParts);
        CHECK(!in.healRobotLimbsOnOrganics);
    }

    void ParseFormulaAcceptsBothModes()
    {
        const char* name = "ParseHealingFormula";
        CHECK(RegenRules::ParseFormula("Toughness", HEALING_FORMULA_LEGACY) == HEALING_FORMULA_TOUGHNESS);
        CHECK(RegenRules::ParseFormula("legacy", HEALING_FORMULA_TOUGHNESS) == HEALING_FORMULA_LEGACY);
        CHECK(RegenRules::ParseFormula("nope", HEALING_FORMULA_LEGACY) == HEALING_FORMULA_LEGACY);
        CHECK(RegenRules::ParseFormula(0, HEALING_FORMULA_TOUGHNESS) == HEALING_FORMULA_TOUGHNESS);
    }

    void SimulationDeltaScalesWithGameSpeed()
    {
        const char* name = "SimulationDeltaScalesWithGameSpeed";
        float dt = -1.0f;
        CHECK(RegenRules::SimulationDelta(0.016f, 1.0f, false, dt));
        CHECK_NEAR(dt, 0.016f, 1e-6f);
        CHECK(RegenRules::SimulationDelta(0.016f, 3.0f, false, dt));
        CHECK_NEAR(dt, 0.048f, 1e-6f);
        CHECK(RegenRules::SimulationDelta(0.016f, 100.0f, false, dt));
        CHECK_NEAR(dt, 1.6f, 1e-5f);
        CHECK(RegenRules::SimulationDelta(0.016f, 0.0f, false, dt));
        CHECK_NEAR(dt, 0.0f, 1e-6f);
    }

    void SimulationDeltaSkipsPausedAndBogusSpeeds()
    {
        const char* name = "SimulationDeltaSkipsPausedAndBogusSpeeds";
        float dt = -1.0f;
        CHECK(!RegenRules::SimulationDelta(0.016f, 1.0f, true, dt));
        CHECK(!RegenRules::SimulationDelta(0.016f, -1.0f, false, dt));
        CHECK(!RegenRules::SimulationDelta(0.016f, 100.5f, false, dt));
        CHECK(!RegenRules::SimulationDelta(0.016f, std::numeric_limits<float>::quiet_NaN(), false, dt));
        CHECK(!RegenRules::SimulationDelta(std::numeric_limits<float>::infinity(), 1.0f, false, dt));
        CHECK(!RegenRules::SimulationDelta(-0.016f, 1.0f, false, dt));
    }

    void WatchdogUsesHealingFactorThreshold()
    {
        const char* name = "WatchdogUsesHealingFactorThreshold";
        // Max 100: threshold = max(0.5, 0.5) = 0.5 HP.
        CHECK(!RegenRules::IsUnexplainedFleshDrop(50.0f, 49.6f, 100.0f));
        CHECK(RegenRules::IsUnexplainedFleshDrop(50.0f, 49.4f, 100.0f));
        // Max 220: threshold = max(1.1, 0.5) = 1.1 HP.
        CHECK(!RegenRules::IsUnexplainedFleshDrop(50.0f, 49.0f, 220.0f));
        CHECK(RegenRules::IsUnexplainedFleshDrop(50.0f, 48.8f, 220.0f));
        // Max 40: threshold floors at 0.5 HP.
        CHECK(!RegenRules::IsUnexplainedFleshDrop(10.0f, 9.6f, 40.0f));
        CHECK(RegenRules::IsUnexplainedFleshDrop(10.0f, 9.4f, 40.0f));
        // Healing (a rise) is never damage.
        CHECK(!RegenRules::IsUnexplainedFleshDrop(10.0f, 30.0f, 100.0f));
        // Fractions below zero (dying part) are still compared exactly.
        CHECK(RegenRules::IsUnexplainedFleshDrop(-1.0f, -2.0f, 100.0f));
    }

    void CooldownCountsDownToZero()
    {
        const char* name = "CooldownCountsDownToZero";
        CHECK_NEAR(RegenRules::TickCooldown(5.0f, 1.5f), 3.5f, 1e-6f);
        CHECK_NEAR(RegenRules::TickCooldown(1.0f, 1.5f), 0.0f, 1e-6f);
        CHECK_NEAR(RegenRules::TickCooldown(0.0f, 1.0f), 0.0f, 1e-6f);
    }

    void OrganicPartsHealStumpsDoNot()
    {
        const char* name = "OrganicPartsHealStumpsDoNot";
        PartHealInputs in = Organic();
        CHECK(RegenRules::ShouldHealPart(in));
        in.isStump = true;
        CHECK(!RegenRules::ShouldHealPart(in));
    }

    void SkeletonPartsHealByDefault()
    {
        const char* name = "SkeletonPartsHealByDefault";
        PartHealInputs in = Organic();
        in.isRobotic = true;
        in.raceIsRobot = true;
        CHECK(RegenRules::ShouldHealPart(in));
        in.healSkeletonParts = false;
        CHECK(!RegenRules::ShouldHealPart(in));
        // A skeleton's severed limb is never healed directly (skeletons never regrow).
        in.healSkeletonParts = true;
        in.isStump = true;
        CHECK(!RegenRules::ShouldHealPart(in));
    }

    void ProstheticsOnOrganicsDoNotHealByDefault()
    {
        const char* name = "ProstheticsOnOrganicsDoNotHealByDefault";
        PartHealInputs in = Organic();
        in.isRobotic = true;
        CHECK(!RegenRules::ShouldHealPart(in));
        in.healRobotLimbsOnOrganics = true;
        CHECK(RegenRules::ShouldHealPart(in));
        // HealSkeletonParts does not govern prosthetics on organics.
        in.healRobotLimbsOnOrganics = false;
        in.healSkeletonParts = true;
        CHECK(!RegenRules::ShouldHealPart(in));
    }

    void FleshStopsAtTheCeiling()
    {
        const char* name = "FleshStopsAtTheCeiling";
        CHECK_NEAR(RegenRules::HealFleshTowardsCeiling(10.0f, 2.0f, 50.0f), 12.0f, 1e-6f);
        CHECK_NEAR(RegenRules::HealFleshTowardsCeiling(49.5f, 2.0f, 50.0f), 50.0f, 1e-6f);
        CHECK_NEAR(RegenRules::HealFleshTowardsCeiling(50.0f, 2.0f, 50.0f), 50.0f, 1e-6f);
        // Above the ceiling (healed by a doctor): never reduced.
        CHECK_NEAR(RegenRules::HealFleshTowardsCeiling(80.0f, 2.0f, 50.0f), 80.0f, 1e-6f);
        // A dying part (negative flesh) climbs back.
        CHECK_NEAR(RegenRules::HealFleshTowardsCeiling(-20.0f, 2.0f, 50.0f), -18.0f, 1e-6f);
        CHECK_NEAR(RegenRules::HealFleshTowardsCeiling(10.0f, 0.0f, 50.0f), 10.0f, 1e-6f);
    }

    void DamageLayersRecoverToZero()
    {
        const char* name = "DamageLayersRecoverToZero";
        CHECK_NEAR(RegenRules::RecoverDamageLayer(30.0f, 2.0f), 28.0f, 1e-6f);
        CHECK_NEAR(RegenRules::RecoverDamageLayer(1.0f, 2.0f), 0.0f, 1e-6f);
        CHECK_NEAR(RegenRules::RecoverDamageLayer(0.0f, 2.0f), 0.0f, 1e-6f);
    }

    void LegacyRateKeepsTheOldBehaviour()
    {
        const char* name = "LegacyRateKeepsTheOldBehaviour";
        CHECK_NEAR(RegenRules::LegacyRatePerSecond(100.0f, 60.0f, false), 100.0f / 60.0f, 1e-5f);
        CHECK_NEAR(RegenRules::LegacyRatePerSecond(200.0f, 60.0f, true), 100.0f / 60.0f, 1e-5f);
        // 0 seconds meant "instant" in the old path (division by zero); keep it instant, but finite.
        CHECK(RegenRules::LegacyRatePerSecond(100.0f, 0.0f, false) >= 100.0f);
        CHECK_NEAR(RegenRules::LegacyRatePerSecond(0.0f, 60.0f, false), 0.0f, 1e-6f);
    }

    void PartRegenStartsOnlyBelowTheThreshold()
    {
        const char* name = "PartRegenStartsOnlyBelowTheThreshold";
        // Below the threshold the part heals.
        CHECK(RegenRules::ShouldStartPartRegen(40.0f, 100.0f, 0.5f));
        CHECK(RegenRules::ShouldStartPartRegen(-20.0f, 100.0f, 0.5f));
        // At or above it the part does not.
        CHECK(!RegenRules::ShouldStartPartRegen(50.0f, 100.0f, 0.5f));
        CHECK(!RegenRules::ShouldStartPartRegen(70.0f, 100.0f, 0.5f));
        CHECK(!RegenRules::ShouldStartPartRegen(100.0f, 100.0f, 0.5f));
        // 1.0 (or more) always heals: the old behaviour.
        CHECK(RegenRules::ShouldStartPartRegen(100.0f, 100.0f, 1.0f));
        CHECK(RegenRules::ShouldStartPartRegen(70.0f, 100.0f, 1.5f));
        // A part without a max never heals.
        CHECK(!RegenRules::ShouldStartPartRegen(10.0f, 0.0f, 1.0f));
        CHECK(!RegenRules::ShouldStartPartRegen(10.0f, -5.0f, 0.5f));
        // 0 (or less) never heals, even a badly hurt part.
        CHECK(!RegenRules::ShouldStartPartRegen(-50.0f, 100.0f, 0.0f));
        CHECK(!RegenRules::ShouldStartPartRegen(10.0f, 100.0f, -1.0f));
        // The configured value is clamped to [0, 1].
        CHECK_NEAR(RegenRules::ClampStartBelowFraction(0.5f), 0.5f, 1e-6f);
        CHECK_NEAR(RegenRules::ClampStartBelowFraction(-0.2f), 0.0f, 1e-6f);
        CHECK_NEAR(RegenRules::ClampStartBelowFraction(3.0f), 1.0f, 1e-6f);
        CHECK_NEAR(RegenRules::ClampStartBelowFraction(std::numeric_limits<float>::quiet_NaN()), 1.0f, 1e-6f);
    }

    void RegistryKeepsStatePerIdentity()
    {
        const char* name = "RegistryKeepsStatePerIdentity";
        int alice = 0, bob = 0, partA = 0;
        RegenStateRegistry registry;
        const unsigned long long aliceId = RegenStateRegistry::MakeIdentity(7, 1);

        CHECK(registry.Find(&alice, aliceId) == 0);
        CharacterRegenState& state = registry.Get(&alice, aliceId);
        CHECK_NEAR(state.damageCooldown, 0.0f, 1e-6f);
        state.damageCooldown = 5.0f;
        state.previousFlesh[&partA] = 40.0f;

        CHECK_NEAR(registry.Get(&alice, aliceId).damageCooldown, 5.0f, 1e-6f);
        CHECK(registry.Find(&alice, aliceId) != 0);
        registry.Get(&bob, RegenStateRegistry::MakeIdentity(8, 1));
        CHECK(registry.Size() == 2);
    }

    void RegistryResetsAReusedPointer()
    {
        const char* name = "RegistryResetsAReusedPointer";
        int slot = 0, partA = 0;
        RegenStateRegistry registry;
        CharacterRegenState& first = registry.Get(&slot, RegenStateRegistry::MakeIdentity(7, 1));
        first.damageCooldown = 5.0f;
        first.previousFlesh[&partA] = 40.0f;

        // Same pointer, new handle serial: a different character.
        const unsigned long long otherId = RegenStateRegistry::MakeIdentity(7, 2);
        CHECK(registry.Find(&slot, otherId) == 0);
        CharacterRegenState& second = registry.Get(&slot, otherId);
        CHECK_NEAR(second.damageCooldown, 0.0f, 1e-6f);
        CHECK(second.previousFlesh.empty());
        CHECK(registry.Size() == 1);
    }

    void RegistryForgetsRemovedCharacters()
    {
        const char* name = "RegistryForgetsRemovedCharacters";
        int alice = 0;
        RegenStateRegistry registry;
        registry.Get(&alice, RegenStateRegistry::MakeIdentity(1, 1));
        registry.Forget(&alice);
        CHECK(registry.Size() == 0);
        registry.Forget(&alice); // forgetting twice is harmless
        CHECK(registry.Size() == 0);
        CHECK(RegenStateRegistry::MakeIdentity(1, 2) != RegenStateRegistry::MakeIdentity(2, 1));
    }
}

void RunRegenRulesTests()
{
    DefaultPartInputsAreAnOrganicPart();
    ParseFormulaAcceptsBothModes();
    SimulationDeltaScalesWithGameSpeed();
    SimulationDeltaSkipsPausedAndBogusSpeeds();
    WatchdogUsesHealingFactorThreshold();
    CooldownCountsDownToZero();
    OrganicPartsHealStumpsDoNot();
    SkeletonPartsHealByDefault();
    ProstheticsOnOrganicsDoNotHealByDefault();
    FleshStopsAtTheCeiling();
    DamageLayersRecoverToZero();
    LegacyRateKeepsTheOldBehaviour();
    PartRegenStartsOnlyBelowTheThreshold();
    RegistryKeepsStatePerIdentity();
    RegistryResetsAReusedPointer();
    RegistryForgetsRemovedCharacters();
}
