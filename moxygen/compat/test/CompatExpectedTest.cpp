/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/Expected.h>

#include <doctest/doctest.h>
#include <string>

namespace moxygen::compat::test {

// Test error type
enum class TestError { NotFound, InvalidInput, Timeout };

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("Expected: default construct") {
  Expected<int, TestError> e;
  CHECK(e.hasValue());
  CHECK_EQ(e.value(), 0);
}

TEST_CASE("Expected: construct with value") {
  Expected<int, TestError> e(42);
  CHECK(e.hasValue());
  CHECK_EQ(e.value(), 42);
}

TEST_CASE("Expected: construct with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));
  CHECK(e.hasError());
  CHECK_EQ(e.error(), TestError::NotFound);
}

TEST_CASE("Expected: converting construct") {
  Expected<long, TestError> e(42);  // int to long
  CHECK(e.hasValue());
  CHECK_EQ(e.value(), 42L);
}

TEST_CASE("Expected: copy construct") {
  Expected<int, TestError> e1(42);
  Expected<int, TestError> e2(e1);

  CHECK(e2.hasValue());
  CHECK_EQ(e2.value(), 42);
}

TEST_CASE("Expected: move construct") {
  Expected<std::string, TestError> e1("hello");
  Expected<std::string, TestError> e2(std::move(e1));

  CHECK(e2.hasValue());
  CHECK_EQ(e2.value(), "hello");
}

// =============================================================================
// Observer Tests
// =============================================================================

TEST_CASE("Expected: hasValue") {
  Expected<int, TestError> good(42);
  Expected<int, TestError> bad(makeUnexpected(TestError::InvalidInput));

  CHECK(good.hasValue());
  CHECK_FALSE(bad.hasValue());
}

TEST_CASE("Expected: hasError") {
  Expected<int, TestError> good(42);
  Expected<int, TestError> bad(makeUnexpected(TestError::InvalidInput));

  CHECK_FALSE(good.hasError());
  CHECK(bad.hasError());
}

TEST_CASE("Expected: bool conversion") {
  Expected<int, TestError> good(42);
  Expected<int, TestError> bad(makeUnexpected(TestError::InvalidInput));

  CHECK(static_cast<bool>(good));
  CHECK_FALSE(static_cast<bool>(bad));
}

TEST_CASE("Expected: has_value alias") {
  Expected<int, TestError> good(42);
  CHECK(good.has_value());  // C++23 style
}

// =============================================================================
// Value Access Tests
// =============================================================================

TEST_CASE("Expected: value lvalue") {
  Expected<int, TestError> e(42);
  CHECK_EQ(e.value(), 42);
}

TEST_CASE("Expected: value const lvalue") {
  const Expected<int, TestError> e(42);
  CHECK_EQ(e.value(), 42);
}

TEST_CASE("Expected: value rvalue") {
  Expected<std::string, TestError> e("hello");
  std::string s = std::move(e).value();
  CHECK_EQ(s, "hello");
}

TEST_CASE("Expected: value on error throws") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));
  CHECK_THROWS_AS(e.value(), std::logic_error);
}

TEST_CASE("Expected: dereference operator") {
  Expected<int, TestError> e(42);
  CHECK_EQ(*e, 42);
}

TEST_CASE("Expected: arrow operator") {
  Expected<std::string, TestError> e("hello");
  CHECK_EQ(e->length(), 5);
}

// =============================================================================
// Error Access Tests
// =============================================================================

TEST_CASE("Expected: error lvalue") {
  Expected<int, TestError> e(makeUnexpected(TestError::Timeout));
  CHECK_EQ(e.error(), TestError::Timeout);
}

TEST_CASE("Expected: error const lvalue") {
  const Expected<int, TestError> e(makeUnexpected(TestError::Timeout));
  CHECK_EQ(e.error(), TestError::Timeout);
}

TEST_CASE("Expected: error rvalue") {
  Expected<int, std::string> e(makeUnexpected(std::string("error msg")));
  std::string err = std::move(e).error();
  CHECK_EQ(err, "error msg");
}

// =============================================================================
// value_or / valueOr Tests
// =============================================================================

TEST_CASE("Expected: value_or with value") {
  Expected<int, TestError> e(42);
  CHECK_EQ(e.value_or(100), 42);
}

TEST_CASE("Expected: value_or with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));
  CHECK_EQ(e.value_or(100), 100);
}

TEST_CASE("Expected: value_or rvalue") {
  Expected<std::string, TestError> e(makeUnexpected(TestError::NotFound));
  CHECK_EQ(std::move(e).value_or("default"), "default");
}

TEST_CASE("Expected: valueOr folly style") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));
  CHECK_EQ(e.valueOr(999), 999);
}

// =============================================================================
// error_or Tests
// =============================================================================

TEST_CASE("Expected: error_or with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::Timeout));
  CHECK_EQ(e.error_or(TestError::NotFound), TestError::Timeout);
}

TEST_CASE("Expected: error_or with value") {
  Expected<int, TestError> e(42);
  CHECK_EQ(e.error_or(TestError::NotFound), TestError::NotFound);
}

// =============================================================================
// then / transform Tests
// =============================================================================

TEST_CASE("Expected: then with value") {
  Expected<int, TestError> e(21);

  auto result = e.then([](int x) { return x * 2; });

  CHECK(result.hasValue());
  CHECK_EQ(result.value(), 42);
}

TEST_CASE("Expected: then with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));

  auto result = e.then([](int x) { return x * 2; });

  CHECK(result.hasError());
  CHECK_EQ(result.error(), TestError::NotFound);
}

TEST_CASE("Expected: then type conversion") {
  Expected<int, TestError> e(42);

  auto result = e.then([](int x) { return std::to_string(x); });

  CHECK(result.hasValue());
  CHECK_EQ(result.value(), "42");
}

TEST_CASE("Expected: then chaining") {
  Expected<int, TestError> e(10);

  auto result = e
    .then([](int x) { return x + 5; })
    .then([](int x) { return x * 2; })
    .then([](int x) { return std::to_string(x); });

  CHECK_EQ(result.value(), "30");
}

TEST_CASE("Expected: transform alias") {
  Expected<int, TestError> e(21);

  auto result = e.transform([](int x) { return x * 2; });

  CHECK_EQ(result.value(), 42);
}

// =============================================================================
// and_then Tests
// =============================================================================

TEST_CASE("Expected: and_then with value") {
  Expected<int, TestError> e(42);

  auto result = e.and_then([](int x) -> Expected<std::string, TestError> {
    return std::to_string(x);
  });

  CHECK(result.hasValue());
  CHECK_EQ(result.value(), "42");
}

TEST_CASE("Expected: and_then with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));

  auto result = e.and_then([](int x) -> Expected<std::string, TestError> {
    return std::to_string(x);
  });

  CHECK(result.hasError());
  CHECK_EQ(result.error(), TestError::NotFound);
}

TEST_CASE("Expected: and_then returning error") {
  Expected<int, TestError> e(42);

  auto result = e.and_then([](int x) -> Expected<std::string, TestError> {
    if (x > 0) {
      return makeUnexpected(TestError::InvalidInput);
    }
    return std::to_string(x);
  });

  CHECK(result.hasError());
  CHECK_EQ(result.error(), TestError::InvalidInput);
}

TEST_CASE("Expected: and_then chaining") {
  auto divide = [](int a, int b) -> Expected<int, TestError> {
    if (b == 0) {
      return makeUnexpected(TestError::InvalidInput);
    }
    return a / b;
  };

  Expected<int, TestError> e(100);

  auto result = e
    .and_then([&divide](int x) { return divide(x, 2); })
    .and_then([&divide](int x) { return divide(x, 5); });

  CHECK(result.hasValue());
  CHECK_EQ(result.value(), 10);
}

// =============================================================================
// or_else Tests
// =============================================================================

TEST_CASE("Expected: or_else with value") {
  Expected<int, TestError> e(42);

  auto result = e.or_else([](TestError) -> Expected<int, TestError> {
    return 999;
  });

  CHECK(result.hasValue());
  CHECK_EQ(result.value(), 42);
}

TEST_CASE("Expected: or_else with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));

  auto result = e.or_else([](TestError) -> Expected<int, TestError> {
    return 999;  // Recovery
  });

  CHECK(result.hasValue());
  CHECK_EQ(result.value(), 999);
}

TEST_CASE("Expected: or_else propagates different error") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));

  auto result = e.or_else([](TestError) -> Expected<int, TestError> {
    return makeUnexpected(TestError::Timeout);  // Different error
  });

  CHECK(result.hasError());
  CHECK_EQ(result.error(), TestError::Timeout);
}

// =============================================================================
// transform_error Tests
// =============================================================================

TEST_CASE("Expected: transform_error with value") {
  Expected<int, TestError> e(42);

  auto result = e.transform_error([](TestError err) {
    return static_cast<int>(err);
  });

  CHECK(result.hasValue());
  CHECK_EQ(result.value(), 42);
}

TEST_CASE("Expected: transform_error with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::Timeout));

  auto result = e.transform_error([](TestError err) {
    return std::string("Error: ") + std::to_string(static_cast<int>(err));
  });

  CHECK(result.hasError());
  CHECK_EQ(result.error(), "Error: 2");  // Timeout = 2
}

// =============================================================================
// onError Tests
// =============================================================================

TEST_CASE("Expected: onError with value") {
  Expected<int, TestError> e(42);

  bool called = false;
  e.onError([&called](TestError) { called = true; });

  CHECK_FALSE(called);
}

TEST_CASE("Expected: onError with error") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));

  TestError captured = TestError::Timeout;
  e.onError([&captured](TestError err) { captured = err; });

  CHECK_EQ(captured, TestError::NotFound);
}

TEST_CASE("Expected: onError chaining") {
  Expected<int, TestError> e(makeUnexpected(TestError::NotFound));

  TestError captured = TestError::Timeout;
  auto& result = e.onError([&captured](TestError err) { captured = err; });

  // Should return reference to self for chaining
  CHECK_EQ(&result, &e);
}

// =============================================================================
// Comparison Tests
// =============================================================================

TEST_CASE("Expected: equality value-value") {
  Expected<int, TestError> e1(42);
  Expected<int, TestError> e2(42);
  Expected<int, TestError> e3(99);

  CHECK(e1 == e2);
  CHECK_FALSE(e1 == e3);
}

TEST_CASE("Expected: equality error-error") {
  Expected<int, TestError> e1(makeUnexpected(TestError::NotFound));
  Expected<int, TestError> e2(makeUnexpected(TestError::NotFound));
  Expected<int, TestError> e3(makeUnexpected(TestError::Timeout));

  CHECK(e1 == e2);
  CHECK_FALSE(e1 == e3);
}

TEST_CASE("Expected: equality value-error") {
  Expected<int, TestError> e1(42);
  Expected<int, TestError> e2(makeUnexpected(TestError::NotFound));

  CHECK_FALSE(e1 == e2);
}

TEST_CASE("Expected: inequality") {
  Expected<int, TestError> e1(42);
  Expected<int, TestError> e2(99);

  CHECK(e1 != e2);
}

// =============================================================================
// Unexpected Tests
// =============================================================================

TEST_CASE("Unexpected: value") {
  Unexpected<TestError> u(TestError::InvalidInput);
  CHECK_EQ(u.value(), TestError::InvalidInput);
}

TEST_CASE("makeUnexpected") {
  auto u = makeUnexpected(TestError::Timeout);
  CHECK_EQ(u.value(), TestError::Timeout);
}

// =============================================================================
// Complex Type Tests
// =============================================================================

TEST_CASE("Expected: complex value type") {
  struct Data {
    std::string name;
    int value;
  };

  Expected<Data, TestError> e(Data{"test", 42});

  CHECK(e.hasValue());
  CHECK_EQ(e->name, "test");
  CHECK_EQ(e->value, 42);
}

TEST_CASE("Expected: move-only value type") {
  Expected<std::unique_ptr<int>, TestError> e(std::make_unique<int>(42));

  CHECK(e.hasValue());
  auto ptr = std::move(e).value();
  CHECK_EQ(*ptr, 42);
}

} // namespace moxygen::compat::test
