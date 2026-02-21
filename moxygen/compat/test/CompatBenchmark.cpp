/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * Performance benchmarks for compat/ module
 *
 * These benchmarks measure critical path performance for:
 * - Buffer operations (allocation, copy, trim)
 * - Cursor operations (read, write)
 * - Hash functions
 * - Async primitives
 * - Priority queue operations
 *
 * Run with: ./CompatBenchmark
 */

#include <moxygen/compat/Async.h>
#include <moxygen/compat/Hash.h>

#if !MOXYGEN_USE_FOLLY
#include <moxygen/compat/ByteBuffer.h>
#include <moxygen/compat/ByteBufferQueue.h>
#include <moxygen/compat/ByteCursor.h>
#include <moxygen/compat/MoQPriorityQueue.h>
#endif

#include <doctest/doctest.h>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

namespace moxygen::compat::bench {

// Simple timing utilities
class Timer {
 public:
  void start() {
    start_ = std::chrono::high_resolution_clock::now();
  }

  double elapsedMs() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start_).count();
  }

  double elapsedUs() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(end - start_).count();
  }

  double elapsedNs() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(end - start_).count();
  }

 private:
  std::chrono::high_resolution_clock::time_point start_;
};

static void printBenchmark(const std::string& name, int iterations, double totalMs) {
  double avgUs = (totalMs * 1000.0) / iterations;
  double opsPerSec = iterations / (totalMs / 1000.0);

  std::cout << "  " << name << ": "
            << avgUs << " us/op, "
            << opsPerSec << " ops/sec"
            << std::endl;
}

// =============================================================================
// Async Benchmarks
// =============================================================================

TEST_CASE("Benchmark: Promise/SemiFuture") {
  const int iterations = 100000;
  Timer timer;

  std::cout << "\n=== Promise/SemiFuture Benchmarks ===" << std::endl;

  // Benchmark: Create promise, get future, set value, get value
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    Promise<int> promise;
    auto future = promise.getSemiFuture();
    promise.setValue(i);
    auto val = future.value();
    (void)val;
  }
  printBenchmark("Promise->setValue->value cycle", iterations, timer.elapsedMs());

  // Benchmark: Immediate SemiFuture
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    SemiFuture<int> future(i);
    auto val = future.value();
    (void)val;
  }
  printBenchmark("Immediate SemiFuture", iterations, timer.elapsedMs());

  // Benchmark: thenValue chain
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    SemiFuture<int> future(10);
    auto result = future
      .thenValue([](int x) { return x * 2; })
      .thenValue([](int x) { return x + 1; });
    auto val = result.value();
    (void)val;
  }
  printBenchmark("thenValue chain (2 continuations)", iterations, timer.elapsedMs());
}

// =============================================================================
// Hash Benchmarks
// =============================================================================

TEST_CASE("Benchmark: Hash functions") {
  const int iterations = 1000000;
  Timer timer;

  std::cout << "\n=== Hash Function Benchmarks ===" << std::endl;

  // Generate test data
  std::vector<std::string> testStrings;
  for (int i = 0; i < 1000; ++i) {
    testStrings.push_back("test_string_" + std::to_string(i));
  }

  // Benchmark: hash_combine
  timer.start();
  size_t hashSum = 0;
  for (int i = 0; i < iterations; ++i) {
    hashSum += hash_combine(0, i, i * 2);
  }
  printBenchmark("hash_combine (3 values)", iterations, timer.elapsedMs());
  CHECK_NE(hashSum, 0);  // Prevent optimization

  // Benchmark: hashBytes (short string)
  timer.start();
  hashSum = 0;
  for (int i = 0; i < iterations; ++i) {
    hashSum += hashBytes("short", 5);
  }
  printBenchmark("hashBytes (5 bytes)", iterations, timer.elapsedMs());

  // Benchmark: hashBytes (longer string)
  std::string longStr(100, 'x');
  timer.start();
  hashSum = 0;
  for (int i = 0; i < iterations; ++i) {
    hashSum += hashBytes(longStr.data(), longStr.size());
  }
  printBenchmark("hashBytes (100 bytes)", iterations, timer.elapsedMs());

#if !MOXYGEN_USE_FOLLY
  // Benchmark: SecureStringHash (SipHash)
  SecureStringHash secureHasher;
  timer.start();
  hashSum = 0;
  for (int i = 0; i < iterations; ++i) {
    hashSum += secureHasher(testStrings[i % testStrings.size()]);
  }
  printBenchmark("SecureStringHash (SipHash)", iterations, timer.elapsedMs());

  // Benchmark: FnvHash
  FnvHash fnvHasher;
  timer.start();
  hashSum = 0;
  for (int i = 0; i < iterations; ++i) {
    hashSum += fnvHasher(testStrings[i % testStrings.size()]);
  }
  printBenchmark("FnvHash", iterations, timer.elapsedMs());

  // Benchmark: WyHash
  WyHash wyHasher;
  timer.start();
  hashSum = 0;
  for (int i = 0; i < iterations; ++i) {
    hashSum += wyHasher(testStrings[i % testStrings.size()]);
  }
  printBenchmark("WyHash", iterations, timer.elapsedMs());

  // Benchmark: IntegerHash
  IntegerHash intHasher;
  timer.start();
  hashSum = 0;
  for (int i = 0; i < iterations; ++i) {
    hashSum += intHasher(i);
  }
  printBenchmark("IntegerHash", iterations, timer.elapsedMs());
#endif
}

#if !MOXYGEN_USE_FOLLY

// =============================================================================
// ByteBuffer Benchmarks
// =============================================================================

TEST_CASE("Benchmark: ByteBuffer") {
  const int iterations = 100000;
  Timer timer;

  std::cout << "\n=== ByteBuffer Benchmarks ===" << std::endl;

  // Benchmark: Create small buffer (SBO)
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteBuffer buf(32);
    (void)buf;
  }
  printBenchmark("Create small buffer (SBO)", iterations, timer.elapsedMs());

  // Benchmark: Create large buffer (heap)
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteBuffer buf(1024);
    (void)buf;
  }
  printBenchmark("Create large buffer (heap)", iterations, timer.elapsedMs());

  // Benchmark: copyBuffer
  std::string data(100, 'x');
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    auto buf = ByteBuffer::copyBuffer(data);
    (void)buf;
  }
  printBenchmark("copyBuffer (100 bytes)", iterations, timer.elapsedMs());

  // Benchmark: wrapExternal
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    auto buf = ByteBuffer::wrapExternal(data.data(), data.size());
    (void)buf;
  }
  printBenchmark("wrapExternal (zero-copy)", iterations, timer.elapsedMs());

  // Benchmark: trimStart (O(1) operation)
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    auto buf = ByteBuffer::copyBuffer(data);
    buf->trimStart(10);
    buf->trimStart(10);
    buf->trimStart(10);
  }
  printBenchmark("trimStart (3x)", iterations, timer.elapsedMs());

  // Benchmark: clone
  auto srcBuf = ByteBuffer::copyBuffer(data);
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    auto cloned = srcBuf->clone();
    (void)cloned;
  }
  printBenchmark("clone (100 bytes)", iterations, timer.elapsedMs());
}

// =============================================================================
// ByteBufferQueue Benchmarks
// =============================================================================

TEST_CASE("Benchmark: ByteBufferQueue") {
  const int iterations = 100000;
  Timer timer;

  std::cout << "\n=== ByteBufferQueue Benchmarks ===" << std::endl;

  std::string data(100, 'x');

  // Benchmark: append buffer
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteBufferQueue queue;
    queue.append(ByteBuffer::copyBuffer(data));
    queue.append(ByteBuffer::copyBuffer(data));
    queue.append(ByteBuffer::copyBuffer(data));
  }
  printBenchmark("append (3 buffers)", iterations, timer.elapsedMs());

  // Benchmark: move (single buffer)
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteBufferQueue queue;
    queue.append(ByteBuffer::copyBuffer(data));
    auto moved = queue.move();
    (void)moved;
  }
  printBenchmark("move (single buffer, O(1))", iterations, timer.elapsedMs());

  // Benchmark: move (multiple buffers, coalesce)
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteBufferQueue queue;
    queue.append(ByteBuffer::copyBuffer(data));
    queue.append(ByteBuffer::copyBuffer(data));
    auto moved = queue.move();
    (void)moved;
  }
  printBenchmark("move (2 buffers, coalesce)", iterations, timer.elapsedMs());

  // Benchmark: split
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteBufferQueue queue;
    queue.append(ByteBuffer::copyBuffer(data));
    auto split = queue.split(50);
    (void)split;
  }
  printBenchmark("split (50 bytes)", iterations, timer.elapsedMs());

  // Benchmark: chainLength (O(1))
  ByteBufferQueue chainQueue;
  for (int i = 0; i < 100; ++i) {
    chainQueue.append(ByteBuffer::copyBuffer(data));
  }
  timer.start();
  size_t lenSum = 0;
  for (int i = 0; i < iterations * 10; ++i) {
    lenSum += chainQueue.chainLength();
  }
  printBenchmark("chainLength (O(1))", iterations * 10, timer.elapsedMs());
  CHECK_NE(lenSum, 0);
}

// =============================================================================
// ByteCursor Benchmarks
// =============================================================================

TEST_CASE("Benchmark: ByteCursor") {
  const int iterations = 100000;
  Timer timer;

  std::cout << "\n=== ByteCursor Benchmarks ===" << std::endl;

  // Prepare test data
  std::vector<uint8_t> data(1000);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>(i);
  }

  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer(data.data(), data.size()));

  // Benchmark: read<uint32_t>
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteCursor cursor(&queue);
    uint32_t val = cursor.readBE<uint32_t>();
    (void)val;
  }
  printBenchmark("readBE<uint32_t>", iterations, timer.elapsedMs());

  // Benchmark: skip
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteCursor cursor(&queue);
    cursor.skip(100);
  }
  printBenchmark("skip(100)", iterations, timer.elapsedMs());

  // Benchmark: pull
  timer.start();
  uint8_t buf[100];
  for (int i = 0; i < iterations; ++i) {
    ByteCursor cursor(&queue);
    cursor.pull(buf, 100);
  }
  printBenchmark("pull(100)", iterations, timer.elapsedMs());

  // Benchmark: clone
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteCursor cursor(&queue);
    auto cloned = cursor.clone(100);
    (void)cloned;
  }
  printBenchmark("clone(100)", iterations, timer.elapsedMs());

  // Benchmark: peekBytes (zero-copy)
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteCursor cursor(&queue);
    auto range = cursor.peekBytes(100);
    (void)range;
  }
  printBenchmark("peekBytes (zero-copy)", iterations, timer.elapsedMs());

  // Benchmark: cross-buffer read
  ByteBufferQueue multiQueue;
  multiQueue.append(ByteBuffer::copyBuffer(data.data(), 2));
  multiQueue.append(ByteBuffer::copyBuffer(data.data() + 2, 2));
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteCursor cursor(&multiQueue);
    uint32_t val = cursor.readBE<uint32_t>();
    (void)val;
  }
  printBenchmark("readBE<uint32_t> (cross-buffer)", iterations, timer.elapsedMs());
}

// =============================================================================
// MoQPriorityQueue Benchmarks
// =============================================================================

TEST_CASE("Benchmark: MoQPriorityQueue") {
  const int iterations = 100000;
  Timer timer;

  std::cout << "\n=== MoQPriorityQueue Benchmarks ===" << std::endl;

  // Benchmark: enqueue
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    StreamPriorityQueue queue;
    QueuedObject obj;
    obj.groupId = 1;
    obj.objectId = i;
    obj.priority = 3;
    obj.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    queue.enqueue(std::move(obj));
  }
  printBenchmark("enqueue (single object)", iterations, timer.elapsedMs());

  // Benchmark: enqueue/dequeue cycle
  timer.start();
  StreamPriorityQueue cycleQueue;
  for (int i = 0; i < iterations; ++i) {
    QueuedObject obj;
    obj.groupId = 1;
    obj.objectId = i;
    obj.priority = i % 8;
    obj.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    cycleQueue.enqueue(std::move(obj));
    auto dequeued = cycleQueue.dequeue();
    (void)dequeued;
  }
  printBenchmark("enqueue+dequeue cycle", iterations, timer.elapsedMs());

  // Benchmark: priority ordering
  timer.start();
  for (int batch = 0; batch < iterations / 100; ++batch) {
    StreamPriorityQueue priorityQueue;
    for (int i = 0; i < 100; ++i) {
      QueuedObject obj;
      obj.groupId = 1;
      obj.objectId = i;
      obj.priority = i % 8;
      obj.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
      priorityQueue.enqueue(std::move(obj));
    }
    for (int i = 0; i < 100; ++i) {
      auto dequeued = priorityQueue.dequeue();
      (void)dequeued;
    }
  }
  printBenchmark("100 enqueue + 100 dequeue (priority order)", iterations / 100,
                 timer.elapsedMs());

  // Benchmark: stats() call
  StreamPriorityQueue statsQueue;
  for (int i = 0; i < 100; ++i) {
    QueuedObject obj;
    obj.groupId = 1;
    obj.objectId = i;
    statsQueue.enqueue(std::move(obj));
  }
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    auto stats = statsQueue.stats();
    (void)stats;
  }
  printBenchmark("stats()", iterations, timer.elapsedMs());
}

#endif // !MOXYGEN_USE_FOLLY

// =============================================================================
// Memory Allocation Benchmarks
// =============================================================================

TEST_CASE("Benchmark: Memory allocation") {
  const int iterations = 100000;
  Timer timer;

  std::cout << "\n=== Memory Allocation Benchmarks ===" << std::endl;

  // Benchmark: std::make_unique<int[]>
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    auto ptr = std::make_unique<uint8_t[]>(1024);
    (void)ptr;
  }
  printBenchmark("std::make_unique<uint8_t[]>(1024)", iterations, timer.elapsedMs());

  // Benchmark: std::vector resize
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    std::vector<uint8_t> vec;
    vec.resize(1024);
  }
  printBenchmark("std::vector resize(1024)", iterations, timer.elapsedMs());

  // Benchmark: std::string allocation
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    std::string str(1024, 'x');
    (void)str;
  }
  printBenchmark("std::string(1024, 'x')", iterations, timer.elapsedMs());

#if !MOXYGEN_USE_FOLLY
  // Benchmark: ByteBuffer with pool
  timer.start();
  for (int i = 0; i < iterations; ++i) {
    ByteBuffer buf(1024);
    (void)buf;
  }
  printBenchmark("ByteBuffer(1024) with pool", iterations, timer.elapsedMs());
#endif
}

} // namespace moxygen::compat::bench
