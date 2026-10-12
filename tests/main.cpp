// Unit tests for the pure TotalRestore rules (no engine, no KenshiLib).
// Build and run:
//   MSBuild TotalRestore/tests/totalrestore_tests.vcxproj -p:Configuration=Release -p:Platform=x64
//   TotalRestore/tests/x64/Release/totalrestore_tests.exe

#include "test_check.h"
#include "test_suites.h"

namespace TestCheck
{
    int& Failures()
    {
        static int failures = 0;
        return failures;
    }

    int& Checks()
    {
        static int checks = 0;
        return checks;
    }

    void Check(bool ok, const char* testName, const char* what, int line)
    {
        ++Checks();
        if (!ok)
        {
            ++Failures();
            std::printf("  FAIL %s (line %d): %s\n", testName, line, what);
        }
    }

    bool Near(float actual, float expected, float tolerance)
    {
        return std::fabs(actual - expected) <= tolerance;
    }
}

int main()
{
    RunToughnessScalingTests();
    RunLimbMaxRulesTests();
    RunRegenRulesTests();
    RunReviveRulesTests();
    RunRegrowthRulesTests();
    RunFleshCapRulesTests();

    const int checks = TestCheck::Checks();
    const int passed = checks - TestCheck::Failures();
    std::printf("%d/%d checks passed\n", passed, checks);
    return (TestCheck::Failures() == 0) ? 0 : 1;
}
