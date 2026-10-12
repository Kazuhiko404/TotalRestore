// Limb max-HP rules (T2): formula selection, inverse estimate, part filters,
// and the first-insert-wins baseline cache.

#include "LimbMaxRules.h"
#include "LimbBaselineCache.h"
#include "LimbMaxOwnership.h"
#include "test_check.h"
#include "test_suites.h"

#include <limits>

using namespace TotalRestore;

namespace
{
    LimbMaxSettings Scaled()
    {
        LimbMaxSettings s;
        s.formula = LIMB_BONUS_SCALED;
        s.toughnessCap = 100.0f;
        s.bonus.atReference = 30.0f;
        s.bonus.referenceHealth = 100.0f;
        s.bonus.exponent = 0.25f;
        s.bonus.maxPerPart = 40.0f;
        s.healthPercentPerToughness = 0.5f;
        s.maxToughnessMultiplier = 1.5f;
        return s;
    }

    LimbMaxSettings Percent()
    {
        LimbMaxSettings s = Scaled();
        s.formula = LIMB_BONUS_PERCENT;
        return s;
    }

    void DefaultsMatchTheIni()
    {
        const char* name = "DefaultsMatchTheIni";
        const LimbMaxSettings s;
        CHECK(s.formula == LIMB_BONUS_SCALED);
        CHECK_NEAR(s.toughnessCap, 100.0f, 1e-6f);
        CHECK_NEAR(s.bonus.atReference, 30.0f, 1e-6f);
        CHECK_NEAR(s.bonus.referenceHealth, 100.0f, 1e-6f);
        CHECK_NEAR(s.bonus.exponent, 0.25f, 1e-6f);
        CHECK_NEAR(s.bonus.maxPerPart, 40.0f, 1e-6f);
        CHECK_NEAR(s.healthPercentPerToughness, 0.5f, 1e-6f);
        CHECK_NEAR(s.maxToughnessMultiplier, 1.5f, 1e-6f);
    }

    void ParseFormulaAcceptsBothModes()
    {
        const char* name = "ParseFormulaAcceptsBothModes";
        CHECK(LimbMaxRules::ParseFormula("Scaled", LIMB_BONUS_PERCENT) == LIMB_BONUS_SCALED);
        CHECK(LimbMaxRules::ParseFormula("scaled", LIMB_BONUS_PERCENT) == LIMB_BONUS_SCALED);
        CHECK(LimbMaxRules::ParseFormula("Percent", LIMB_BONUS_SCALED) == LIMB_BONUS_PERCENT);
        CHECK(LimbMaxRules::ParseFormula("PERCENT", LIMB_BONUS_SCALED) == LIMB_BONUS_PERCENT);
        CHECK(LimbMaxRules::ParseFormula("garbage", LIMB_BONUS_SCALED) == LIMB_BONUS_SCALED);
        CHECK(LimbMaxRules::ParseFormula("", LIMB_BONUS_PERCENT) == LIMB_BONUS_PERCENT);
        CHECK(LimbMaxRules::ParseFormula(0, LIMB_BONUS_SCALED) == LIMB_BONUS_SCALED);
    }

    void ScaledAddsTheBonusToTheBase()
    {
        const char* name = "ScaledAddsTheBonusToTheBase";
        // Acceptance: T106 with base 220 shows about 244, not 530.
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(220.0f, 106.0f, Scaled()), 244.6f, 0.1f);
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(100.0f, 100.0f, Scaled()), 130.0f, 0.01f);
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(80.0f, 100.0f, Scaled()), 111.7f, 0.1f);
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(100.0f, 0.0f, Scaled()), 100.0f, 1e-4f);
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(100.0f, 50.0f, Scaled()), 115.0f, 0.01f);
    }

    void PercentKeepsTheLegacyMultiplier()
    {
        const char* name = "PercentKeepsTheLegacyMultiplier";
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(220.0f, 106.0f, Percent()), 330.0f, 0.01f);
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(100.0f, 50.0f, Percent()), 125.0f, 0.01f);
    }

    void ComputeLimbMaxRejectsInvalidBase()
    {
        const char* name = "ComputeLimbMaxRejectsInvalidBase";
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(0.0f, 100.0f, Scaled()), 0.0f, 1e-6f);
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(-5.0f, 100.0f, Scaled()), -5.0f, 1e-6f);
        CHECK_NEAR(LimbMaxRules::ComputeLimbMax(0.0f, 100.0f, Percent()), 0.0f, 1e-6f);
    }

    void EstimateBaseInvertsBothFormulas()
    {
        const char* name = "EstimateBaseInvertsBothFormulas";
        const float scaledMax = LimbMaxRules::ComputeLimbMax(220.0f, 106.0f, Scaled());
        CHECK_NEAR(LimbMaxRules::EstimateBaseFromMax(scaledMax, 106.0f, Scaled()), 220.0f, 0.05f);
        const float smallMax = LimbMaxRules::ComputeLimbMax(80.0f, 70.0f, Scaled());
        CHECK_NEAR(LimbMaxRules::EstimateBaseFromMax(smallMax, 70.0f, Scaled()), 80.0f, 0.05f);
        CHECK_NEAR(LimbMaxRules::EstimateBaseFromMax(330.0f, 106.0f, Percent()), 220.0f, 0.01f);
        CHECK_NEAR(LimbMaxRules::EstimateBaseFromMax(100.0f, 0.0f, Scaled()), 100.0f, 1e-4f);
        CHECK_NEAR(LimbMaxRules::EstimateBaseFromMax(0.0f, 100.0f, Scaled()), 0.0f, 1e-6f);
    }

    void ShouldScalePartAppliesFilters()
    {
        const char* name = "ShouldScalePartAppliesFilters";
        CHECK(LimbMaxRules::ShouldScalePart(false, false, false, false));
        CHECK(!LimbMaxRules::ShouldScalePart(true, false, false, true));
        CHECK(LimbMaxRules::ShouldScalePart(true, false, true, true));
        CHECK(!LimbMaxRules::ShouldScalePart(false, true, true, false));
        CHECK(LimbMaxRules::ShouldScalePart(false, true, false, true));
        CHECK(!LimbMaxRules::ShouldScalePart(true, true, false, true));
    }

    void TargetLimbMaxRestoresUnscaledParts()
    {
        const char* name = "TargetLimbMaxRestoresUnscaledParts";
        const LimbMaxSettings s = Scaled();
        // A non-scaled part (e.g. an animal saved while buffed) goes back to its captured base.
        CHECK_NEAR(LimbMaxRules::TargetLimbMax(false, 100.0f, 100.0f, s), 100.0f, 1e-6f);
        CHECK_NEAR(LimbMaxRules::TargetLimbMax(false, 220.0f, 0.0f, Percent()), 220.0f, 1e-6f);
        // A scaled part follows the active formula.
        CHECK_NEAR(LimbMaxRules::TargetLimbMax(true, 100.0f, 100.0f, s),
            LimbMaxRules::ComputeLimbMax(100.0f, 100.0f, s), 1e-6f);
        CHECK_NEAR(LimbMaxRules::TargetLimbMax(true, 100.0f, 100.0f, Percent()), 150.0f, 1e-3f);
        CHECK(LimbMaxRules::TargetLimbMax(true, 100.0f, 50.0f, s) > 100.0f);
    }

    void BaselineCacheIsFirstInsertWins()
    {
        const char* name = "BaselineCacheIsFirstInsertWins";
        int partA = 0, partB = 0, raceHuman = 0, raceShek = 0, prosthetic = 0;
        LimbBaselineCache cache;
        float base = -1.0f;

        CHECK(!cache.Find(LimbBaselineKey(&partA, &raceHuman, 0), base));
        CHECK_NEAR(cache.Remember(LimbBaselineKey(&partA, &raceHuman, 0), 100.0f), 100.0f, 1e-6f);
        // A later (already buffed) value never replaces the captured base.
        CHECK_NEAR(cache.Remember(LimbBaselineKey(&partA, &raceHuman, 0), 130.0f), 100.0f, 1e-6f);
        CHECK(cache.Find(LimbBaselineKey(&partA, &raceHuman, 0), base));
        CHECK_NEAR(base, 100.0f, 1e-6f);

        // Race, part and prosthetic variant each make a distinct key.
        CHECK(!cache.Find(LimbBaselineKey(&partA, &raceShek, 0), base));
        CHECK(!cache.Find(LimbBaselineKey(&partB, &raceHuman, 0), base));
        CHECK(!cache.Find(LimbBaselineKey(&partA, &raceHuman, &prosthetic), base));
        CHECK_NEAR(cache.Remember(LimbBaselineKey(&partA, &raceShek, 0), 220.0f), 220.0f, 1e-6f);
        CHECK(cache.Size() == 2);
    }

    void BaselineCacheIgnoresInvalidValues()
    {
        const char* name = "BaselineCacheIgnoresInvalidValues";
        int part = 0;
        LimbBaselineCache cache;
        float base = -1.0f;
        CHECK_NEAR(cache.Remember(LimbBaselineKey(&part, 0, 0), 0.0f), 0.0f, 1e-6f);
        CHECK_NEAR(cache.Remember(LimbBaselineKey(&part, 0, 0), -3.0f), -3.0f, 1e-6f);
        CHECK(!cache.Find(LimbBaselineKey(&part, 0, 0), base));
        CHECK_NEAR(cache.Remember(LimbBaselineKey(0, 0, 0), 50.0f), 50.0f, 1e-6f);
        CHECK(!cache.Find(LimbBaselineKey(0, 0, 0), base));
        CHECK(cache.Size() == 0);
        cache.Remember(LimbBaselineKey(&part, 0, 0), 80.0f);
        cache.Clear();
        CHECK(cache.Size() == 0);
    }

    // C2: TotalRestore yields _maxHealth to another mod (DesolationSystem) that rewrites it.
    void ForeignWriteNeedsAChangedMaxOnTheSameLimb()
    {
        const char* name = "ForeignWriteNeedsAChangedMaxOnTheSameLimb";
        int prosthetic = 0;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        // Unchanged since our write (float noise included): still ours.
        CHECK(!LimbMaxOwnership::IsForeignWrite(130.0f, 0, 130.0f, 0));
        CHECK(!LimbMaxOwnership::IsForeignWrite(130.0f, 0, 130.0005f, 0));
        // Another mod rewrote the max, up or down.
        CHECK(LimbMaxOwnership::IsForeignWrite(130.0f, 0, 162.0f, 0));
        CHECK(LimbMaxOwnership::IsForeignWrite(130.0f, 0, 120.0f, 0));
        // A prosthetic was attached or removed: the native swap rewrote it, not a foreign mod.
        CHECK(!LimbMaxOwnership::IsForeignWrite(130.0f, 0, 60.0f, &prosthetic));
        CHECK(!LimbMaxOwnership::IsForeignWrite(60.0f, &prosthetic, 130.0f, 0));
        // Invalid values never take ownership away.
        CHECK(!LimbMaxOwnership::IsForeignWrite(130.0f, 0, nan, 0));
        CHECK(!LimbMaxOwnership::IsForeignWrite(nan, 0, 130.0f, 0));
        CHECK(!LimbMaxOwnership::IsForeignWrite(0.0f, 0, 130.0f, 0));
    }

    void OwnershipStartsOursAndFollowsOurWrites()
    {
        const char* name = "OwnershipStartsOursAndFollowsOurWrites";
        LimbMaxOwnership ownership;
        int part = 0;
        // Never written by us: nothing to yield (no-op for every part we have not touched).
        CHECK(!ownership.IsForeignOwned(&part, 162.0f, 0));
        ownership.RecordWrite(&part, 130.0f, 0);
        CHECK(!ownership.IsForeignOwned(&part, 130.0f, 0));
        CHECK(ownership.IsForeignOwned(&part, 162.0f, 0));
        // Our next write (e.g. after the foreign mod stopped) takes it back.
        ownership.RecordWrite(&part, 162.0f, 0);
        CHECK(!ownership.IsForeignOwned(&part, 162.0f, 0));
        CHECK(ownership.Size() == 1);
    }

    void OwnershipResetsWhenNativeSetupRuns()
    {
        const char* name = "OwnershipResetsWhenNativeSetupRuns";
        LimbMaxOwnership ownership;
        int a = 0;
        int b = 0;
        ownership.RecordWrite(&a, 130.0f, 0);
        ownership.RecordWrite(&b, 90.0f, 0);
        ownership.Release(&a);
        CHECK(!ownership.IsForeignOwned(&a, 162.0f, 0));
        CHECK(ownership.IsForeignOwned(&b, 120.0f, 0));
        ownership.Release(&a); // releasing twice is harmless
        CHECK(ownership.Size() == 1);
        ownership.RecordWrite(0, 130.0f, 0); // NULL parts are ignored
        CHECK(ownership.Size() == 1);
        ownership.Clear();
        CHECK(ownership.Size() == 0);
    }

    void OwnershipStaysBounded()
    {
        const char* name = "OwnershipStaysBounded";
        LimbMaxOwnership ownership;
        static char parts[LimbMaxOwnership::kMaxRecords + 1];
        for (size_t i = 0; i <= LimbMaxOwnership::kMaxRecords; ++i) ownership.RecordWrite(&parts[i], 100.0f, 0);
        // The table is dropped when full: at worst one extra write per part, never unbounded growth.
        CHECK(ownership.Size() <= LimbMaxOwnership::kMaxRecords);
        CHECK(!ownership.IsForeignOwned(&parts[LimbMaxOwnership::kMaxRecords], 100.0f, 0));
    }
}

void RunLimbMaxRulesTests()
{
    DefaultsMatchTheIni();
    ParseFormulaAcceptsBothModes();
    ScaledAddsTheBonusToTheBase();
    PercentKeepsTheLegacyMultiplier();
    ComputeLimbMaxRejectsInvalidBase();
    EstimateBaseInvertsBothFormulas();
    ShouldScalePartAppliesFilters();
    TargetLimbMaxRestoresUnscaledParts();
    BaselineCacheIsFirstInsertWins();
    BaselineCacheIgnoresInvalidValues();
    ForeignWriteNeedsAChangedMaxOnTheSameLimb();
    OwnershipStartsOursAndFollowsOurWrites();
    OwnershipResetsWhenNativeSetupRuns();
    OwnershipStaysBounded();
}
