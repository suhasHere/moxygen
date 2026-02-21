/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/Async.h>

#include <doctest/doctest.h>
#include <chrono>
#include <thread>
#include <atomic>

namespace moxygen::compat::test {

// =============================================================================
// Promise<T> Tests
// =============================================================================

TEST_CASE("Promise: setValue and getValue") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  CHECK_FALSE(future.isReady());
  promise.setValue(42);
  CHECK(future.isReady());
  CHECK_EQ(future.value(), 42);
}

TEST_CASE("Promise: setValue string") {
  Promise<std::string> promise;
  auto future = promise.getSemiFuture();

  promise.setValue("hello world");
  CHECK(future.isReady());
  CHECK_EQ(future.value(), "hello world");
}

TEST_CASE("Promise: setValue move-only type") {
  Promise<std::unique_ptr<int>> promise;
  auto future = promise.getSemiFuture();

  promise.setValue(std::make_unique<int>(100));
  CHECK(future.isReady());
  auto ptr = future.value();
  CHECK_EQ(*ptr, 100);
}

TEST_CASE("Promise: double setValue throws") {
  Promise<int> promise;
  promise.setValue(1);
  CHECK_THROWS_AS(promise.setValue(2), std::runtime_error);
}

TEST_CASE("Promise: double getSemiFuture throws") {
  Promise<int> promise;
  auto f1 = promise.getSemiFuture();
  CHECK_THROWS_AS(promise.getSemiFuture(), std::runtime_error);
}

TEST_CASE("Promise: isFulfilled") {
  Promise<int> promise;
  CHECK_FALSE(promise.isFulfilled());
  promise.setValue(42);
  CHECK(promise.isFulfilled());
}

// =============================================================================
// Promise<void> Tests
// =============================================================================

TEST_CASE("Promise<void>: setValue") {
  Promise<void> promise;
  auto future = promise.getSemiFuture();

  CHECK_FALSE(future.isReady());
  promise.setValue();
  CHECK(future.isReady());
  CHECK_NOTHROW(future.value());
}

TEST_CASE("Promise<void>: setValue with Unit") {
  Promise<void> promise;
  auto future = promise.getSemiFuture();

  promise.setValue(unit);
  CHECK(future.isReady());
}

TEST_CASE("Promise<void>: double setValue throws") {
  Promise<void> promise;
  promise.setValue();
  CHECK_THROWS_AS(promise.setValue(), std::runtime_error);
}

// =============================================================================
// SemiFuture<T> Tests
// =============================================================================

TEST_CASE("SemiFuture: immediate value") {
  SemiFuture<int> future(42);
  CHECK(future.isReady());
  CHECK(future.valid());
  CHECK_EQ(future.value(), 42);
}

TEST_CASE("SemiFuture: wait") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  std::thread t([&promise]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    promise.setValue(123);
  });

  future.wait();
  CHECK(future.isReady());
  CHECK_EQ(future.value(), 123);
  t.join();
}

TEST_CASE("SemiFuture: wait_for") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  // Should timeout
  CHECK_FALSE(future.wait_for(std::chrono::milliseconds(5)));
  CHECK_FALSE(future.isReady());

  promise.setValue(42);

  // Should succeed
  CHECK(future.wait_for(std::chrono::milliseconds(100)));
  CHECK(future.isReady());
}

TEST_CASE("SemiFuture: value with timeout") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  // Should return nullopt on timeout
  auto result = future.value(std::chrono::milliseconds(5));
  CHECK_FALSE(result.has_value());

  promise.setValue(99);

  // Create a new future since value was not consumed
  Promise<int> promise2;
  auto future2 = promise2.getSemiFuture();
  promise2.setValue(99);
  auto result2 = future2.value(std::chrono::milliseconds(100));
  CHECK(result2.has_value());
  CHECK_EQ(*result2, 99);
}

TEST_CASE("SemiFuture: single consumption") {
  SemiFuture<int> future(42);

  CHECK_EQ(future.value(), 42);
  // Second consumption should throw
  CHECK_THROWS_AS(future.value(), std::runtime_error);
}

TEST_CASE("SemiFuture: no state") {
  SemiFuture<int> future;
  CHECK_FALSE(future.valid());
  CHECK_FALSE(future.isReady());
  CHECK_THROWS_AS(future.value(), std::runtime_error);
}

// =============================================================================
// Exception Propagation Tests
// =============================================================================

TEST_CASE("Promise: setException") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  promise.setException(std::make_exception_ptr(std::runtime_error("test error")));
  CHECK(future.isReady());
  CHECK_THROWS_AS(future.value(), std::runtime_error);
}

TEST_CASE("Promise<void>: setException") {
  Promise<void> promise;
  auto future = promise.getSemiFuture();

  promise.setException(std::make_exception_ptr(std::logic_error("void error")));
  CHECK(future.isReady());
  CHECK_THROWS_AS(future.value(), std::logic_error);
}

TEST_CASE("SemiFuture: immediate exception") {
  auto ex = std::make_exception_ptr(std::runtime_error("immediate"));
  SemiFuture<int> future(ex);

  CHECK(future.isReady());
  CHECK_THROWS_AS(future.value(), std::runtime_error);
}

// =============================================================================
// Continuation (thenValue) Tests
// =============================================================================

TEST_CASE("thenValue: basic") {
  SemiFuture<int> future(10);

  auto doubled = future.thenValue([](int x) { return x * 2; });
  CHECK(doubled.isReady());
  CHECK_EQ(doubled.value(), 20);
}

TEST_CASE("thenValue: chain") {
  SemiFuture<int> future(5);

  auto result = future
    .thenValue([](int x) { return x + 1; })
    .thenValue([](int x) { return x * 3; })
    .thenValue([](int x) { return std::to_string(x); });

  CHECK_EQ(result.value(), "18");
}

TEST_CASE("thenValue: deferred execution") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  std::atomic<bool> continuationRan{false};
  auto chained = future.thenValue([&continuationRan](int x) {
    continuationRan = true;
    return x * 2;
  });

  CHECK_FALSE(continuationRan);
  promise.setValue(21);

  // Continuation should run after setValue
  CHECK(chained.isReady());
  CHECK_EQ(chained.value(), 42);
}

TEST_CASE("thenValue: propagates exception") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  auto chained = future.thenValue([](int x) { return x * 2; });

  promise.setException(std::make_exception_ptr(std::runtime_error("error")));

  CHECK(chained.isReady());
  CHECK_THROWS_AS(chained.value(), std::runtime_error);
}

TEST_CASE("thenValue: throws in continuation") {
  SemiFuture<int> future(10);

  auto chained = future.thenValue([](int) -> int {
    throw std::logic_error("continuation error");
  });

  CHECK(chained.isReady());
  CHECK_THROWS_AS(chained.value(), std::logic_error);
}

TEST_CASE("thenValue: void continuation") {
  SemiFuture<int> future(42);

  std::atomic<int> capturedValue{0};
  auto voidFuture = future.thenValue([&capturedValue](int x) {
    capturedValue = x;
  });

  voidFuture.value();
  CHECK_EQ(capturedValue, 42);
}

// =============================================================================
// SemiFuture<void> Tests
// =============================================================================

TEST_CASE("SemiFuture<void>: immediate") {
  SemiFuture<void> future(unit);
  CHECK(future.isReady());
  CHECK_NOTHROW(future.value());
}

TEST_CASE("SemiFuture<void>: thenValue returning int") {
  SemiFuture<void> future(unit);

  auto result = future.thenValue([]() { return 42; });
  CHECK_EQ(result.value(), 42);
}

TEST_CASE("SemiFuture<void>: thenValue returning void") {
  SemiFuture<void> future(unit);

  std::atomic<bool> ran{false};
  auto chained = future.thenValue([&ran]() { ran = true; });

  chained.value();
  CHECK(ran);
}

// =============================================================================
// Factory Function Tests
// =============================================================================

TEST_CASE("makeSemiFuture: with value") {
  auto future = makeSemiFuture(42);
  CHECK(future.isReady());
  CHECK_EQ(future.value(), 42);
}

TEST_CASE("makeSemiFuture: void") {
  auto future = makeSemiFuture();
  CHECK(future.isReady());
}

// Note: makeSemiFutureException is only available in std mode
#if !MOXYGEN_USE_FOLLY
TEST_CASE("makeSemiFutureException") {
  auto future = makeSemiFutureException<int>(std::runtime_error("factory error"));
  CHECK(future.isReady());
  CHECK_THROWS_AS(future.value(), std::runtime_error);
}
#endif

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_CASE("Concurrent set and get") {
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  std::atomic<int> result{0};

  std::thread producer([&promise]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    promise.setValue(42);
  });

  std::thread consumer([&future, &result]() {
    result = future.value();
  });

  producer.join();
  consumer.join();

  CHECK_EQ(result, 42);
}

TEST_CASE("Multiple producer race") {
  // Test that only one producer can set value
  Promise<int> promise;
  auto future = promise.getSemiFuture();

  std::atomic<int> successCount{0};

  auto trySet = [&promise, &successCount](int val) {
    try {
      promise.setValue(val);
      successCount++;
    } catch (const std::runtime_error&) {
      // Expected for all but one thread
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back(trySet, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  CHECK_EQ(successCount, 1);
  CHECK(future.isReady());
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_CASE("Promise: move construct") {
  Promise<int> promise1;
  auto future = promise1.getSemiFuture();

  Promise<int> promise2(std::move(promise1));
  promise2.setValue(42);

  CHECK_EQ(future.value(), 42);
}

TEST_CASE("Promise: move assign") {
  Promise<int> promise1;
  auto future = promise1.getSemiFuture();

  Promise<int> promise2;
  promise2 = std::move(promise1);
  promise2.setValue(99);

  CHECK_EQ(future.value(), 99);
}

} // namespace moxygen::compat::test
