#pragma once

#include <iostream>
#include <string>

// Deliberately hand-rolled instead of pulling in Catch2/GoogleTest via FetchContent - this
// project has exactly two small test binaries, and a real dependency isn't worth the network
// fetch and build-time cost for that.
namespace test {

inline int& failureCount() {
    static int count = 0;
    return count;
}

inline int& totalCount() {
    static int count = 0;
    return count;
}

template <typename A, typename B>
void expectEq(const A& actual, const B& expected, const std::string& what, const char* file, int line) {
    totalCount()++;
    if (!(actual == expected)) {
        failureCount()++;
        std::cerr << "FAIL " << file << ":" << line << " - " << what << "\n";
        std::cerr << "  actual:   " << actual << "\n";
        std::cerr << "  expected: " << expected << "\n";
    }
}

inline void expectTrue(bool condition, const std::string& what, const char* file, int line) {
    totalCount()++;
    if (!condition) {
        failureCount()++;
        std::cerr << "FAIL " << file << ":" << line << " - " << what << "\n";
    }
}

}  // namespace test

#define EXPECT_EQ(actual, expected) \
    ::test::expectEq((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)
#define EXPECT_TRUE(condition) ::test::expectTrue((condition), #condition, __FILE__, __LINE__)
