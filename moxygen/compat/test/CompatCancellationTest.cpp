/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/Cancellation.h>

#include <doctest/doctest.h>
#include <atomic>
#include <thread>
#include <vector>

namespace moxygen::compat::test {

// =============================================================================
// CancellationSource Basic Tests
// =============================================================================

TEST_CASE("CancellationSource: creation") {
  CancellationSource source;
  CHECK_FALSE(source.isCancellationRequested());
}

TEST_CASE("CancellationSource: requestCancellation") {
  CancellationSource source;

  bool firstCall = source.requestCancellation();
  CHECK(firstCall);
  CHECK(source.isCancellationRequested());

  // Second call should return false
  bool secondCall = source.requestCancellation();
  CHECK_FALSE(secondCall);
}

TEST_CASE("CancellationSource: copy") {
  CancellationSource source1;
  CancellationSource source2(source1);

  source1.requestCancellation();

  // Both should be cancelled (shared state)
  CHECK(source1.isCancellationRequested());
  CHECK(source2.isCancellationRequested());
}

TEST_CASE("CancellationSource: move") {
  CancellationSource source1;
  auto token = source1.getToken();

  CancellationSource source2(std::move(source1));
  source2.requestCancellation();

  CHECK(token.isCancellationRequested());
}

// =============================================================================
// CancellationToken Basic Tests
// =============================================================================

TEST_CASE("CancellationToken: from source") {
  CancellationSource source;
  CancellationToken token = source.getToken();

  CHECK_FALSE(token.isCancellationRequested());
  CHECK(token.canBeCancelled());

  source.requestCancellation();

  CHECK(token.isCancellationRequested());
}

TEST_CASE("CancellationToken: default") {
  CancellationToken token;
  CHECK_FALSE(token.isCancellationRequested());
  CHECK_FALSE(token.canBeCancelled());
}

TEST_CASE("CancellationToken: multiple tokens from source") {
  CancellationSource source;
  CancellationToken token1 = source.getToken();
  CancellationToken token2 = source.getToken();
  CancellationToken token3 = source.getToken();

  source.requestCancellation();

  CHECK(token1.isCancellationRequested());
  CHECK(token2.isCancellationRequested());
  CHECK(token3.isCancellationRequested());
}

TEST_CASE("CancellationToken: independent sources") {
  CancellationSource source1;
  CancellationSource source2;

  auto token1 = source1.getToken();
  auto token2 = source2.getToken();

  source1.requestCancellation();

  CHECK(token1.isCancellationRequested());
  CHECK_FALSE(token2.isCancellationRequested());
}

// =============================================================================
// CancellationCallback Tests
// =============================================================================

TEST_CASE("CancellationCallback: invoked") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<bool> called{false};
  CancellationCallback callback(token, [&called]() { called = true; });

  CHECK_FALSE(called);

  source.requestCancellation();

  CHECK(called);
}

TEST_CASE("CancellationCallback: invoked immediately if already cancelled") {
  CancellationSource source;
  auto token = source.getToken();

  source.requestCancellation();

  std::atomic<bool> called{false};
  CancellationCallback callback(token, [&called]() { called = true; });

  // Callback should be invoked immediately
  CHECK(called);
}

TEST_CASE("CancellationCallback: unregistered on destruction") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<int> callCount{0};

  {
    CancellationCallback callback(token, [&callCount]() { callCount++; });
    // callback goes out of scope here
  }

  source.requestCancellation();

  // Callback should not be invoked (was unregistered)
  CHECK_EQ(callCount, 0);
}

TEST_CASE("CancellationCallback: manual unregister") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<bool> called{false};
  CancellationCallback callback(token, [&called]() { called = true; });

  callback.unregister();

  source.requestCancellation();

  CHECK_FALSE(called);
}

TEST_CASE("CancellationCallback: multiple callbacks") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<int> callCount{0};

  CancellationCallback cb1(token, [&callCount]() { callCount++; });
  CancellationCallback cb2(token, [&callCount]() { callCount++; });
  CancellationCallback cb3(token, [&callCount]() { callCount++; });

  source.requestCancellation();

  CHECK_EQ(callCount, 3);
}

TEST_CASE("CancellationCallback: with null token") {
  CancellationToken token;  // Default token

  std::atomic<bool> called{false};
  CancellationCallback callback(token, [&called]() { called = true; });

  // Nothing to cancel, callback should never be called
  CHECK_FALSE(called);
}

TEST_CASE("CancellationCallback: move") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<bool> called{false};
  CancellationCallback cb1(token, [&called]() { called = true; });

  CancellationCallback cb2(std::move(cb1));

  source.requestCancellation();

  CHECK(called);
}

TEST_CASE("CancellationCallback: move assign") {
  CancellationSource source1;
  CancellationSource source2;
  auto token1 = source1.getToken();
  auto token2 = source2.getToken();

  std::atomic<int> callCount1{0};
  std::atomic<int> callCount2{0};

  CancellationCallback cb1(token1, [&callCount1]() { callCount1++; });
  CancellationCallback cb2(token2, [&callCount2]() { callCount2++; });

  cb2 = std::move(cb1);

  // Cancel source1 - should invoke moved callback
  source1.requestCancellation();
  CHECK_EQ(callCount1, 1);
  CHECK_EQ(callCount2, 0);

  // Cancel source2 - old cb2 was replaced, should not invoke
  source2.requestCancellation();
  CHECK_EQ(callCount2, 0);
}

// =============================================================================
// Exception Safety Tests
// =============================================================================

TEST_CASE("CancellationCallback: exception swallowed") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<bool> secondCalled{false};

  CancellationCallback cb1(token, []() {
    throw std::runtime_error("test exception");
  });

  CancellationCallback cb2(token, [&secondCalled]() {
    secondCalled = true;
  });

  // Should not throw, exception is swallowed
  CHECK_NOTHROW(source.requestCancellation());

  // Second callback should still be called
  CHECK(secondCalled);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_CASE("CancellationToken: concurrent cancellation") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<int> successCount{0};

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&source, &successCount]() {
      if (source.requestCancellation()) {
        successCount++;
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Only one thread should succeed
  CHECK_EQ(successCount, 1);
  CHECK(token.isCancellationRequested());
}

TEST_CASE("CancellationToken: concurrent callback registration") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<int> callCount{0};
  std::vector<std::unique_ptr<CancellationCallback>> callbacks;
  std::mutex callbacksMutex;

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&]() {
      auto cb = std::make_unique<CancellationCallback>(
          token, [&callCount]() { callCount++; });
      std::lock_guard<std::mutex> lock(callbacksMutex);
      callbacks.push_back(std::move(cb));
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  source.requestCancellation();

  CHECK_EQ(callCount, 10);
}

TEST_CASE("CancellationToken: concurrent check and cancel") {
  CancellationSource source;
  auto token = source.getToken();

  std::atomic<bool> sawNotCancelled{false};
  std::atomic<bool> sawCancelled{false};

  std::thread checker([&token, &sawNotCancelled, &sawCancelled]() {
    for (int i = 0; i < 10000; ++i) {
      if (token.isCancellationRequested()) {
        sawCancelled = true;
      } else {
        sawNotCancelled = true;
      }
    }
  });

  std::thread canceller([&source]() {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    source.requestCancellation();
  });

  checker.join();
  canceller.join();

  CHECK(token.isCancellationRequested());
}

// =============================================================================
// Lifetime Tests
// =============================================================================

TEST_CASE("CancellationToken: token outlives source") {
  CancellationToken token;

  {
    CancellationSource source;
    token = source.getToken();
    source.requestCancellation();
  }

  // Source is destroyed, but token should still work
  CHECK(token.isCancellationRequested());
}

TEST_CASE("CancellationCallback: lifetime with destroyed source") {
  std::atomic<bool> called{false};

  {
    CancellationSource source;
    auto token = source.getToken();

    CancellationCallback callback(token, [&called]() { called = true; });

    source.requestCancellation();
  }

  CHECK(called);
}

} // namespace moxygen::compat::test
