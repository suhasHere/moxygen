/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/ByteBufferQueue.h>

#include <doctest/doctest.h>
#include <cstring>

#if !MOXYGEN_USE_FOLLY

namespace moxygen::compat::test {

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: default constructor") {
  ByteBufferQueue queue;
  CHECK(queue.empty());
  CHECK_EQ(queue.chainLength(), 0);
}

TEST_CASE("ByteBufferQueue: options constructor") {
  ByteBufferQueue::Options opts{true};
  ByteBufferQueue queue(opts);
  CHECK(queue.empty());
}

TEST_CASE("ByteBufferQueue: cacheChainLength factory") {
  auto opts = ByteBufferQueue::cacheChainLength();
  CHECK(opts.cacheChainLength);
}

// =============================================================================
// Append Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: append buffer") {
  ByteBufferQueue queue;

  auto buf = ByteBuffer::copyBuffer("hello", 5);
  queue.append(std::move(buf));

  CHECK_FALSE(queue.empty());
  CHECK_EQ(queue.chainLength(), 5);
}

TEST_CASE("ByteBufferQueue: append multiple buffers") {
  ByteBufferQueue queue;

  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" ", 1));
  queue.append(ByteBuffer::copyBuffer("world", 5));

  CHECK_EQ(queue.chainLength(), 11);
}

TEST_CASE("ByteBufferQueue: append raw data") {
  ByteBufferQueue queue;

  queue.append("hello", 5);
  CHECK_EQ(queue.chainLength(), 5);

  queue.append(" world", 6);
  CHECK_EQ(queue.chainLength(), 11);
}

TEST_CASE("ByteBufferQueue: append string") {
  ByteBufferQueue queue;

  std::string str = "test string";
  queue.append(str);

  CHECK_EQ(queue.chainLength(), str.size());
}

TEST_CASE("ByteBufferQueue: append empty buffer") {
  ByteBufferQueue queue;

  auto emptyBuf = ByteBuffer::create(0);
  queue.append(std::move(emptyBuf));

  CHECK(queue.empty());
}

TEST_CASE("ByteBufferQueue: append another queue") {
  ByteBufferQueue queue1;
  ByteBufferQueue queue2;

  queue1.append(ByteBuffer::copyBuffer("hello", 5));
  queue2.append(ByteBuffer::copyBuffer(" world", 6));

  queue1.append(std::move(queue2));

  CHECK_EQ(queue1.chainLength(), 11);
  CHECK(queue2.empty());
}

// =============================================================================
// Prepend Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: prepend buffer") {
  ByteBufferQueue queue;

  queue.append(ByteBuffer::copyBuffer("world", 5));
  queue.prepend(ByteBuffer::copyBuffer("hello ", 6));

  CHECK_EQ(queue.chainLength(), 11);

  // First buffer should be "hello "
  CHECK_EQ(std::memcmp(queue.front()->data(), "hello ", 6), 0);
}

TEST_CASE("ByteBufferQueue: prepend another queue") {
  ByteBufferQueue queue1;
  ByteBufferQueue queue2;

  queue1.append(ByteBuffer::copyBuffer("world", 5));
  queue2.append(ByteBuffer::copyBuffer("hello ", 6));

  queue1.prepend(std::move(queue2));

  CHECK_EQ(queue1.chainLength(), 11);
}

// =============================================================================
// WrapBuffer Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: wrapBuffer") {
  ByteBufferQueue queue;

  const char* data = "external data";
  queue.wrapBuffer(data, 13);

  CHECK_EQ(queue.chainLength(), 13);

  // Should be zero-copy (same pointer)
  CHECK_EQ(queue.front()->data(), reinterpret_cast<const uint8_t*>(data));
  CHECK(queue.front()->isExternal());
}

TEST_CASE("ByteBufferQueue: wrapBufferCopy") {
  ByteBufferQueue queue;

  const char* data = "copy data";
  queue.wrapBufferCopy(data, 9);

  CHECK_EQ(queue.chainLength(), 9);

  // Should be a copy (different pointer)
  CHECK_NE(queue.front()->data(), reinterpret_cast<const uint8_t*>(data));
  CHECK_FALSE(queue.front()->isExternal());
}

// =============================================================================
// Move Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: move single buffer") {
  ByteBufferQueue queue;

  queue.append(ByteBuffer::copyBuffer("test", 4));

  auto buf = queue.move();

  CHECK_NE(buf, nullptr);
  CHECK_EQ(buf->length(), 4);
  CHECK_EQ(buf->toString(), "test");
  CHECK(queue.empty());
}

TEST_CASE("ByteBufferQueue: move multiple buffers") {
  ByteBufferQueue queue;

  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  auto buf = queue.move();

  CHECK_NE(buf, nullptr);
  CHECK_EQ(buf->length(), 11);
  CHECK_EQ(buf->toString(), "hello world");
  CHECK(queue.empty());
}

TEST_CASE("ByteBufferQueue: move empty") {
  ByteBufferQueue queue;
  auto buf = queue.move();
  CHECK_EQ(buf, nullptr);
}

TEST_CASE("ByteBufferQueue: moveAsChainView") {
  ByteBufferQueue queue;

  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  auto view = queue.moveAsChainView();

  CHECK_EQ(view.chainLength(), 11);
  CHECK_EQ(view.size(), 2);  // Two buffers
  CHECK(queue.empty());
}

// =============================================================================
// Split Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: split within first buffer") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  auto split = queue.split(5);

  CHECK_EQ(split->length(), 5);
  CHECK_EQ(split->toString(), "hello");
  CHECK_EQ(queue.chainLength(), 6);
}

TEST_CASE("ByteBufferQueue: split exact first buffer") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  auto split = queue.split(5);

  CHECK_EQ(split->length(), 5);
  CHECK_EQ(split->toString(), "hello");
  CHECK_EQ(queue.chainLength(), 6);
}

TEST_CASE("ByteBufferQueue: split across buffers") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  auto split = queue.split(7);

  CHECK_EQ(split->length(), 7);
  CHECK_EQ(split->toString(), "hello w");
  CHECK_EQ(queue.chainLength(), 4);
}

TEST_CASE("ByteBufferQueue: split zero") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));

  auto split = queue.split(0);

  CHECK_EQ(split, nullptr);
  CHECK_EQ(queue.chainLength(), 4);
}

TEST_CASE("ByteBufferQueue: split more than available") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));

  auto split = queue.split(100);

  CHECK_EQ(split->length(), 4);
  CHECK(queue.empty());
}

TEST_CASE("ByteBufferQueue: splitChain") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("aaa", 3));
  queue.append(ByteBuffer::copyBuffer("bbb", 3));
  queue.append(ByteBuffer::copyBuffer("ccc", 3));

  auto chain = queue.splitChain(6);

  CHECK_EQ(chain.chainLength(), 6);
  CHECK_EQ(queue.chainLength(), 3);
}

// =============================================================================
// TrimStart/TrimEnd Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: trimStart single buffer") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  queue.trimStart(6);

  CHECK_EQ(queue.chainLength(), 5);
}

TEST_CASE("ByteBufferQueue: trimStart multiple buffers") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" ", 1));
  queue.append(ByteBuffer::copyBuffer("world", 5));

  queue.trimStart(7);

  CHECK_EQ(queue.chainLength(), 4);
}

TEST_CASE("ByteBufferQueue: trimStart all") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));

  queue.trimStart(100);

  CHECK(queue.empty());
}

TEST_CASE("ByteBufferQueue: trimEnd single buffer") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  queue.trimEnd(6);

  CHECK_EQ(queue.chainLength(), 5);
}

TEST_CASE("ByteBufferQueue: trimEnd multiple buffers") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" ", 1));
  queue.append(ByteBuffer::copyBuffer("world", 5));

  queue.trimEnd(7);

  CHECK_EQ(queue.chainLength(), 4);
}

// =============================================================================
// Front Access Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: front") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));

  CHECK_NE(queue.front(), nullptr);
  CHECK_EQ(queue.front()->length(), 4);
}

TEST_CASE("ByteBufferQueue: front empty") {
  ByteBufferQueue queue;
  CHECK_EQ(queue.front(), nullptr);
}

TEST_CASE("ByteBufferQueue: frontData") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));

  CHECK_NE(queue.frontData(), nullptr);
  CHECK_EQ(std::memcmp(queue.frontData(), "test", 4), 0);
}

// =============================================================================
// IOVec/Scatter-Gather Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: gather") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  auto iovecs = queue.gather();

  CHECK_EQ(iovecs.size(), 2);
  CHECK_EQ(iovecs[0].length, 5);
  CHECK_EQ(iovecs[1].length, 6);
}

TEST_CASE("ByteBufferQueue: gather empty") {
  ByteBufferQueue queue;
  auto iovecs = queue.gather();
  CHECK(iovecs.empty());
}

#ifndef _WIN32
TEST_CASE("ByteBufferQueue: gatherNative") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));

  auto iovecs = queue.gatherNative();

  CHECK_EQ(iovecs.size(), 1);
  CHECK_EQ(iovecs[0].iov_len, 4);
}
#endif

// =============================================================================
// ChainView Tests
// =============================================================================

TEST_CASE("ChainView: iteration") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("a", 1));
  queue.append(ByteBuffer::copyBuffer("b", 1));
  queue.append(ByteBuffer::copyBuffer("c", 1));

  auto view = queue.moveAsChainView();

  int count = 0;
  for (const auto& buf : view) {
    count++;
    CHECK_EQ(buf->length(), 1);
  }
  CHECK_EQ(count, 3);
}

TEST_CASE("ChainView: coalesce") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  auto view = queue.moveAsChainView();
  auto coalesced = view.coalesce();

  CHECK_EQ(coalesced->length(), 11);
  CHECK_EQ(coalesced->toString(), "hello world");
}

TEST_CASE("ChainView: coalesce single") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("single", 6));

  auto view = queue.moveAsChainView();
  auto coalesced = view.coalesce();

  // Single buffer should be moved, not copied
  CHECK_EQ(coalesced->length(), 6);
  CHECK_EQ(coalesced->toString(), "single");
}

TEST_CASE("ChainView: getIovecs") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("aaa", 3));
  queue.append(ByteBuffer::copyBuffer("bbb", 3));

  auto view = queue.moveAsChainView();
  auto iovecs = view.getIovecs();

  CHECK_EQ(iovecs.size(), 2);
  CHECK_EQ(iovecs[0].length, 3);
  CHECK_EQ(iovecs[1].length, 3);
}

// =============================================================================
// Preallocate/Postallocate Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: preallocate/postallocate") {
  ByteBufferQueue queue;

  auto [ptr, capacity] = queue.preallocate(10, 100);

  CHECK_NE(ptr, nullptr);
  CHECK_GE(capacity, 100);
  CHECK(queue.hasPendingPreallocate());

  // Write some data
  std::memcpy(ptr, "test data", 9);

  queue.postallocate(9);

  CHECK_FALSE(queue.hasPendingPreallocate());
  CHECK_EQ(queue.chainLength(), 9);
}

TEST_CASE("ByteBufferQueue: preallocate min too large") {
  ByteBufferQueue queue;

  CHECK_THROWS_AS(queue.preallocate(200, 100), std::invalid_argument);
}

TEST_CASE("ByteBufferQueue: double preallocate throws") {
  ByteBufferQueue queue;

  queue.preallocate(10, 100);
  CHECK_THROWS_AS(queue.preallocate(10, 100), std::logic_error);
}

TEST_CASE("ByteBufferQueue: postallocate without preallocate throws") {
  ByteBufferQueue queue;
  CHECK_THROWS_AS(queue.postallocate(10), std::logic_error);
}

TEST_CASE("ByteBufferQueue: postallocate exceeds capacity throws") {
  ByteBufferQueue queue;

  queue.preallocate(10, 100);
  CHECK_THROWS_AS(queue.postallocate(200), std::invalid_argument);
}

TEST_CASE("ByteBufferQueue: cancelPreallocate") {
  ByteBufferQueue queue;

  queue.preallocate(10, 100);
  CHECK(queue.hasPendingPreallocate());

  queue.cancelPreallocate();

  CHECK_FALSE(queue.hasPendingPreallocate());
  CHECK(queue.empty());
}

TEST_CASE("ByteBufferQueue: postallocatePartial") {
  ByteBufferQueue queue;

  auto [ptr, capacity] = queue.preallocate(10, 100);
  std::memcpy(ptr, "test", 4);

  auto [remainingPtr, remainingCap] = queue.postallocatePartial(4);

  CHECK_EQ(queue.chainLength(), 4);
}

// =============================================================================
// Clone Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: clone") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  auto cloned = queue.clone();

  CHECK_EQ(cloned.chainLength(), 11);
  CHECK_EQ(queue.chainLength(), 11);  // Original unchanged
}

TEST_CASE("ByteBufferQueue: cloneOne") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("first", 5));
  queue.append(ByteBuffer::copyBuffer("second", 6));

  auto clonedFirst = queue.cloneOne();

  CHECK_NE(clonedFirst, nullptr);
  CHECK_EQ(clonedFirst->length(), 5);
  CHECK_EQ(clonedFirst->toString(), "first");
}

// =============================================================================
// Clear Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: clear") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));
  queue.preallocate(10, 100);

  queue.clear();

  CHECK(queue.empty());
  CHECK_EQ(queue.chainLength(), 0);
  CHECK_FALSE(queue.hasPendingPreallocate());
}

// =============================================================================
// ChainLength Verification Tests
// =============================================================================

TEST_CASE("ByteBufferQueue: chainLength consistency") {
  ByteBufferQueue queue;

  queue.append(ByteBuffer::copyBuffer("aaa", 3));
  CHECK_EQ(queue.chainLength(), 3);
  CHECK(queue.verifyCachedLength());

  queue.append(ByteBuffer::copyBuffer("bbb", 3));
  CHECK_EQ(queue.chainLength(), 6);
  CHECK(queue.verifyCachedLength());

  queue.trimStart(2);
  CHECK_EQ(queue.chainLength(), 4);
  CHECK(queue.verifyCachedLength());

  queue.trimEnd(1);
  CHECK_EQ(queue.chainLength(), 3);
  CHECK(queue.verifyCachedLength());
}

} // namespace moxygen::compat::test

#endif // !MOXYGEN_USE_FOLLY
