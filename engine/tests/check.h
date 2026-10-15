// Minimal test helper shared by all engine tests. Each test file defines main() via RUN_TESTS.
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace check {
inline int failures = 0;
inline std::vector<std::pair<std::string, std::function<void()>>>& registry() {
    static std::vector<std::pair<std::string, std::function<void()>>> tests;
    return tests;
}
struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().emplace_back(name, std::move(fn)); }
};
}  // namespace check

#define TEST(name)                                                   \
    static void name();                                              \
    static check::Registrar name##_registrar(#name, name);           \
    static void name()

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ++check::failures;                                                       \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
        }                                                                            \
    } while (0)

#define CHECK_EQ(a, b)                                                                     \
    do {                                                                                   \
        auto _a = (a);                                                                     \
        auto _b = (b);                                                                     \
        if (!(_a == _b)) {                                                                 \
            ++check::failures;                                                             \
            std::ostringstream _m;                                                         \
            _m << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #a " == " #b "\n"   \
               << "  left:  " << _a << "\n  right: " << _b << "\n";                        \
            std::cerr << _m.str();                                                         \
        }                                                                                  \
    } while (0)

#define RUN_TESTS()                                                        \
    int main() {                                                           \
        for (auto& [name, fn] : check::registry()) {                       \
            int before = check::failures;                                  \
            fn();                                                          \
            std::cout << (check::failures == before ? "PASS " : "FAIL ") << name << "\n"; \
        }                                                                  \
        std::cout << (check::failures ? "FAILED" : "OK") << " (" << check::failures << " failures)\n"; \
        return check::failures ? 1 : 0;                                    \
    }
