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

} // namespace tinytest

#define TEST(name) \
    static void name(); \
    static ::tinytest::Registrar reg_##name(#name, name); \
    static void name()

#define ASSERT_TRUE(x) do { if(!(x)) throw ::tinytest::AssertFailure(std::string("ASSERT_TRUE failed: ") + #x); } while(0)
#define ASSERT_FALSE(x) do { if((x)) throw ::tinytest::AssertFailure(std::string("ASSERT_FALSE failed: ") + #x); } while(0)
#define ASSERT_EQ(a,b) do { if(!((a)==(b))) { \
  std::ostringstream _oss; _oss << "ASSERT_EQ failed: " << #a << " == " << #b <<"("<<(a)<<"!="<<(b)<< ") | L: " << __LINE__; \
  throw ::tinytest::AssertFailure(_oss.str()); } } while(0)
#define ASSERT_NE(a,b) do { if(!((a)!=(b))) { \
  std::ostringstream _oss; _oss << "ASSERT_NE failed: " << #a << " != " << #b <<"("<<(a)<<"=="<<(b)<< ") | L: " << __LINE__; \
  throw ::tinytest::AssertFailure(_oss.str()); } } while(0)
