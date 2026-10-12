#pragma once

// Minimal check macros for the TotalRestore unit tests (no framework).
// Each test function declares `const char* name = "...";` so CHECK can report it.

#include <cmath>
#include <cstdio>

namespace TestCheck
{
    int& Failures();
    int& Checks();
    void Check(bool ok, const char* testName, const char* what, int line);
    bool Near(float actual, float expected, float tolerance);
}

#define CHECK(cond) TestCheck::Check((cond), name, #cond, __LINE__)
#define CHECK_NEAR(actual, expected, tol) \
    TestCheck::Check(TestCheck::Near((actual), (expected), (tol)), name, #actual " ~= " #expected, __LINE__)
