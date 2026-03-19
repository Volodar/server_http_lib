// Minimal header-only test framework for unit/integration tests
#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>

namespace tinytest {

struct TestCase { std::string name; std::function<void()> fn; };

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r; return r;
}

inline void register_test(const std::string& name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        register_test(name, std::move(fn));
    }
};

struct AssertFailure : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline int run_tests() {
    int failed = 0;
    int passed = 0;
    for (const auto& tc : tinytest::registry()) {
        try {
            tc.fn();
            ++passed;
            std::cout << "[ OK ] " << tc.name << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "[FAIL] " << tc.name << ": " << e.what() << "\n";
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << tc.name << ": unknown error\n";
        }
    }
    std::cout << "Passed: " << passed << ", Failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}

} // namespace tinytest

#define TEST(name) \
    static void name(); \
    static ::tinytest::Registrar reg_##name(#name, name); \
    static void name()

#define ASSERT_TRUE(x) do { if(!(x)) { \
    std::ostringstream _oss; _oss << "ASSERT_TRUE failed: " << #x " | L: " << __LINE__; \
    throw ::tinytest::AssertFailure(_oss.str()); } } while(0)

#define ASSERT_FALSE(x) do { if((x)) { \
    std::ostringstream _oss; _oss << "ASSERT_FALSE failed: " << #x " | L: " << __LINE__; \
    throw ::tinytest::AssertFailure(_oss.str()); } } while(0)

#define ASSERT_NULL(x) do { if((x) != nullptr) {\
    std::ostringstream _oss; _oss << "ASSERT_NULL failed: " << #x " | L: " << __LINE__; \
    throw ::tinytest::AssertFailure(std::string("ASSERT_NULL failed: ") + #x); } } while(0)
#define ASSERT_NOT_NULL(x) do { if((x) == nullptr) {\
    std::ostringstream _oss; _oss << "ASSERT_NOT_NULL failed: " << #x " | L: " << __LINE__; \
    throw ::tinytest::AssertFailure(std::string("ASSERT_NOT_NULL failed: ") + #x); } } while(0)

#define ASSERT_EQ(a,b) do { if(!((a)==(b))) { \
    std::ostringstream _oss; _oss << "ASSERT_EQ failed: " << #a << " == " << #b <<"("<<(a)<<"!="<<(b)<< ") | L: " << __LINE__; \
    throw ::tinytest::AssertFailure(_oss.str()); } } while(0)

#define ASSERT_NE(a,b) do { if(!((a)!=(b))) { \
  std::ostringstream _oss; _oss << "ASSERT_NE failed: " << #a << " != " << #b <<"("<<(a)<<"=="<<(b)<< ") | L: " << __LINE__; \
  throw ::tinytest::AssertFailure(_oss.str()); } } while(0)
