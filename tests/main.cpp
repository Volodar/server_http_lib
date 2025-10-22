// Shared test runner main
#include "test_framework.h"
#include <iostream>

int main() {
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

