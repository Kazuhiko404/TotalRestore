// Foreign flesh clamp rule (C1 fallback): a drop that only pulls flesh back down
// to the current max is a clamp by another mod, not unreported damage.

#include "FleshCapRules.h"
#include "test_check.h"
#include "test_suites.h"

#include <limits>

using namespace TotalRestore;

namespace
{
    void FleshAtOrBelowTheMaxIsNeverAClamp()
    {
        const char* name = "FleshAtOrBelowTheMaxIsNeverAClamp";
        // Vanilla damage from full or from a wounded part: the old watchdog decides.
        CHECK(!FleshCapRules::IsFleshCapClamp(100.0f, 80.0f, 100.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(60.0f, 40.0f, 100.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(100.0f, 100.0f, 100.0f));
    }

    void DropFromAboveTheMaxToTheMaxIsAClamp()
    {
        const char* name = "DropFromAboveTheMaxToTheMaxIsAClamp";
        // DesolationSystem rewrote _maxHealth lower and clamped flesh to its native cap.
        CHECK(FleshCapRules::IsFleshCapClamp(120.0f, 100.0f, 100.0f));
        // Float rounding of age * HPMult * max - wear on the foreign side.
        CHECK(FleshCapRules::IsFleshCapClamp(250.0f, 199.995f, 200.0f));
        CHECK(FleshCapRules::IsFleshCapClamp(250.0f, 200.004f, 200.0f));
    }

    void DropBelowTheMaxIsStillDamage()
    {
        const char* name = "DropBelowTheMaxIsStillDamage";
        // Clamp plus a real hit in the same tick: the hit must still pause healing.
        CHECK(!FleshCapRules::IsFleshCapClamp(120.0f, 90.0f, 100.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(120.0f, 99.5f, 100.0f));
    }

    void InvalidValuesAreNeverAClamp()
    {
        const char* name = "InvalidValuesAreNeverAClamp";
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        CHECK(!FleshCapRules::IsFleshCapClamp(120.0f, 0.0f, 0.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(10.0f, -5.0f, -5.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(nan, 100.0f, 100.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(120.0f, nan, 100.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(120.0f, 100.0f, nan));
        CHECK(!FleshCapRules::IsFleshCapClamp(inf, 100.0f, 100.0f));
        CHECK(!FleshCapRules::IsFleshCapClamp(120.0f, inf, inf));
    }
}

void RunFleshCapRulesTests()
{
    FleshAtOrBelowTheMaxIsNeverAClamp();
    DropFromAboveTheMaxToTheMaxIsAClamp();
    DropBelowTheMaxIsStillDamage();
    InvalidValuesAreNeverAClamp();
}
