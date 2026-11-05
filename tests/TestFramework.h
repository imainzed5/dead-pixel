#pragma once

// Minimal self-contained test framework (no external dependency).
//
//   DP_TEST(name) { DP_CHECK(cond); DP_CHECK_EQ(a, b); DP_CHECK_NEAR(a, b, eps); }
//
// Run: dead_pixel_tests [substring-filter]

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace dp_test
{
struct TestCase
{
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar
{
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

struct Failure
{
    std::string message;
};

template <typename A, typename B>
std::string describeMismatch(const char* exprA, const char* exprB, const A& a, const B& b)
{
    std::ostringstream out;
    out << exprA << " == " << exprB << " (" << a << " vs " << b << ")";
    return out.str();
}

inline std::string location(const char* file, int line)
{
    return std::string(file) + ":" + std::to_string(line) + ": ";
}
} // namespace dp_test

#define DP_TEST(name)                                                   \
    static void name();                                                 \
    static const dp_test::Registrar name##_registrar(#name, &name);     \
    static void name()

#define DP_CHECK(cond)                                                                       \
    do                                                                                       \
    {                                                                                        \
        if (!(cond))                                                                         \
        {                                                                                    \
            throw dp_test::Failure{dp_test::location(__FILE__, __LINE__) + "CHECK(" #cond ")"}; \
        }                                                                                    \
    } while (0)

#define DP_CHECK_EQ(a, b)                                                                     \
    do                                                                                        \
    {                                                                                         \
        const auto& dpA = (a);                                                                \
        const auto& dpB = (b);                                                                \
        if (!(dpA == dpB))                                                                    \
        {                                                                                     \
            throw dp_test::Failure{dp_test::location(__FILE__, __LINE__) +                    \
                                   dp_test::describeMismatch(#a, #b, dpA, dpB)};              \
        }                                                                                     \
    } while (0)

#define DP_CHECK_NEAR(a, b, eps)                                                              \
    do                                                                                        \
    {                                                                                         \
        const double dpA = static_cast<double>(a);                                            \
        const double dpB = static_cast<double>(b);                                            \
        if (std::fabs(dpA - dpB) > static_cast<double>(eps))                                  \
        {                                                                                     \
            throw dp_test::Failure{dp_test::location(__FILE__, __LINE__) +                    \
                                   dp_test::describeMismatch(#a, #b, dpA, dpB)};              \
        }                                                                                     \
    } while (0)
