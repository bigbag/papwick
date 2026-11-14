#pragma once

#include <cmath>
#include <iostream>
#include <string>

namespace TestUtils {

class TestRunner {
 public:
  explicit TestRunner(const std::string& suiteName) : suite_(suiteName) {}
  ~TestRunner() {
    std::cout << suite_ << ": " << passed_ << " passed, " << failed_ << " failed\n";
  }

  bool expectTrue(bool condition, const std::string& testName) {
    if (condition) {
      ++passed_;
    } else {
      ++failed_;
      std::cout << "FAIL: " << testName << "\n";
    }
    return condition;
  }

  bool expectFalse(bool condition, const std::string& testName) { return expectTrue(!condition, testName); }

  template <typename T, typename U>
  bool expectEq(T expected, U actual, const std::string& testName) {
    const bool ok = expected == actual;
    if (!ok) std::cout << "  expected " << +expected << ", actual " << +actual << "\n";
    return expectTrue(ok, testName);
  }

  bool expectEqual(const std::string& expected, const std::string& actual, const std::string& testName) {
    const bool ok = expected == actual;
    if (!ok) std::cout << "  expected '" << expected << "', actual '" << actual << "'\n";
    return expectTrue(ok, testName);
  }

  bool expectNear(double expected, double actual, double tolerance, const std::string& testName) {
    const bool ok = std::fabs(expected - actual) <= tolerance;
    if (!ok) std::cout << "  expected " << expected << ", actual " << actual << "\n";
    return expectTrue(ok, testName);
  }

  int exitCode() const { return failed_ == 0 ? 0 : 1; }

 private:
  std::string suite_;
  int passed_ = 0;
  int failed_ = 0;
};

}  // namespace TestUtils
