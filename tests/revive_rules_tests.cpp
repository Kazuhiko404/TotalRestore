// Revive rules (T4): death clock keyed by handle fields, window, HP fraction.

#include "ReviveRules.h"
#include "test_check.h"
#include "test_suites.h"

using namespace TotalRestore;

namespace
{
    DeathKey Key(unsigned int index, unsigned int serial)
    {
        return DeathKey(1, 0, 0, index, serial);
    }

    void ClockKeepsTheFirstDeathTime()
    {
        const char* name = "ClockKeepsTheFirstDeathTime";
        DeathClock clock;
        double when = -1.0;
        CHECK(!clock.Find(Key(5, 1), when));
        clock.RecordDeath(Key(5, 1), 100.0);
        clock.RecordDeath(Key(5, 1), 130.0);
        CHECK(clock.Find(Key(5, 1), when));
        CHECK(when == 100.0);
        CHECK(clock.Observe(Key(5, 1), 150.0) == 100.0);
    }

    void ClockUsesFirstObservationAsFallback()
    {
        const char* name = "ClockUsesFirstObservationAsFallback";
        DeathClock clock;
        CHECK(clock.Observe(Key(9, 3), 500.0) == 500.0);
        CHECK(clock.Observe(Key(9, 3), 560.0) == 500.0);
        CHECK(clock.Size() == 1);
    }

    void ClockResetsAFutureRecord()
    {
        const char* name = "ClockResetsAFutureRecord";
        DeathClock clock;
        clock.RecordDeath(Key(5, 1), 1000.0);
        // An earlier save was loaded: the in-game clock is now before the record.
        CHECK(clock.Observe(Key(5, 1), 400.0) == 400.0);
        double when = -1.0;
        CHECK(clock.Find(Key(5, 1), when));
        CHECK(when == 400.0);
    }

    void ClockKeysOnEveryHandleField()
    {
        const char* name = "ClockKeysOnEveryHandleField";
        DeathClock clock;
        clock.RecordDeath(DeathKey(1, 2, 3, 4, 5), 10.0);
        double when = -1.0;
        CHECK(clock.Find(DeathKey(1, 2, 3, 4, 5), when));
        CHECK(!clock.Find(DeathKey(9, 2, 3, 4, 5), when));
        CHECK(!clock.Find(DeathKey(1, 9, 3, 4, 5), when));
        CHECK(!clock.Find(DeathKey(1, 2, 9, 4, 5), when));
        CHECK(!clock.Find(DeathKey(1, 2, 3, 9, 5), when));
        // Same slot index, new serial: a different character reusing the slot.
        CHECK(!clock.Find(DeathKey(1, 2, 3, 4, 9), when));
    }

    void ClockForgetsRevivedCharacters()
    {
        const char* name = "ClockForgetsRevivedCharacters";
        DeathClock clock;
        clock.RecordDeath(Key(5, 1), 10.0);
        clock.Forget(Key(5, 1));
        double when = -1.0;
        CHECK(!clock.Find(Key(5, 1), when));
        CHECK(clock.Size() == 0);
    }

    void WindowMeasuresFromDeath()
    {
        const char* name = "WindowMeasuresFromDeath";
        CHECK(ReviveRules::IsWithinReviveWindow(true, 60.0, 100.0, 100.0));
        CHECK(ReviveRules::IsWithinReviveWindow(true, 60.0, 100.0, 160.0));
        CHECK(!ReviveRules::IsWithinReviveWindow(true, 60.0, 100.0, 160.5));
        CHECK(!ReviveRules::IsWithinReviveWindow(true, 60.0, 100.0, 10000.0));
    }

    void WindowCanBeDisabled()
    {
        const char* name = "WindowCanBeDisabled";
        CHECK(ReviveRules::IsWithinReviveWindow(false, 60.0, 100.0, 10000.0));
        CHECK(ReviveRules::IsWithinReviveWindow(false, 0.0, 0.0, 1.0));
    }

    void WindowToleratesAClockGoingBackwards()
    {
        const char* name = "WindowToleratesAClockGoingBackwards";
        CHECK(ReviveRules::IsWithinReviveWindow(true, 60.0, 500.0, 100.0));
    }

    void ReviveFractionIsClamped()
    {
        const char* name = "ReviveFractionIsClamped";
        CHECK_NEAR(ReviveRules::ClampReviveFraction(0.10f), 0.10f, 1e-6f);
        CHECK_NEAR(ReviveRules::ClampReviveFraction(0.0f), 0.01f, 1e-6f);
        CHECK_NEAR(ReviveRules::ClampReviveFraction(-1.0f), 0.01f, 1e-6f);
        CHECK_NEAR(ReviveRules::ClampReviveFraction(5.0f), 1.0f, 1e-6f);
        CHECK_NEAR(ReviveRules::ClampReviveFraction(1.0f), 1.0f, 1e-6f);
    }
}

void RunReviveRulesTests()
{
    ClockKeepsTheFirstDeathTime();
    ClockUsesFirstObservationAsFallback();
    ClockResetsAFutureRecord();
    ClockKeysOnEveryHandleField();
    ClockForgetsRevivedCharacters();
    WindowMeasuresFromDeath();
    WindowCanBeDisabled();
    WindowToleratesAClockGoingBackwards();
    ReviveFractionIsClamped();
}
