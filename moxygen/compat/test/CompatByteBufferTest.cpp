/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/ByteBuffer.h>

#include <doctest/doctest.h>
#include <cstring>

#if !MOXYGEN_USE_FOLLY

namespace moxygen::compat::test {

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("ByteBuffer: default constructor") {
  ByteBuffer buf;
  CHECK_EQ(buf.length(), 0);
  CHECK(buf.empty());
  CHECK_FALSE(buf.isExternal());
  CHECK(buf.isWritable());
}

TEST_CASE("ByteBuffer: capacity constructor inline") {
  // Small capacity should use inline storage
  ByteBuffer buf(32);
  CHECK_EQ(buf.length(), 0);
  CHECK_GE(buf.capacity(), 32);
}

TEST_CASE("ByteBuffer: capacity constructor heap") {
  // Large capacity should use heap storage
  ByteBuffer buf(1024);
  CHECK_EQ(buf.length(), 0);
  CHECK_GE(buf.capacity(), 1024);
}

TEST_CASE("ByteBuffer: move constructor") {
  auto buf1 = ByteBuffer::copyBuffer("test data", 9);
  size_t originalLen = buf1->length();

  ByteBuffer buf2(std::move(*buf1));

  CHECK_EQ(buf2.length(), originalLen);
  CHECK_EQ(std::memcmp(buf2.data(), "test data", 9), 0);
}

TEST_CASE("ByteBuffer: move assignment") {
  auto buf1 = ByteBuffer::copyBuffer("hello", 5);
  ByteBuffer buf2(100);

  buf2 = std::move(*buf1);

  CHECK_EQ(buf2.length(), 5);
  CHECK_EQ(std::memcmp(buf2.data(), "hello", 5), 0);
}

// =============================================================================
// Factory Methods Tests
// =============================================================================

TEST_CASE("ByteBuffer: create with capacity") {
  auto buf = ByteBuffer::create(512);
  CHECK_NE(buf, nullptr);
  CHECK_GE(buf->capacity(), 512);
  CHECK_EQ(buf->length(), 0);
}

TEST_CASE("ByteBuffer: copyBuffer from data") {
  const char* data = "hello world";
  auto buf = ByteBuffer::copyBuffer(data, 11);

  CHECK_EQ(buf->length(), 11);
  CHECK_EQ(std::memcmp(buf->data(), data, 11), 0);
}

TEST_CASE("ByteBuffer: copyBuffer from string") {
  std::string str = "test string";
  auto buf = ByteBuffer::copyBuffer(str);

  CHECK_EQ(buf->length(), str.size());
  CHECK_EQ(buf->toString(), str);
}

TEST_CASE("ByteBuffer: wrapExternal") {
  const uint8_t data[] = {1, 2, 3, 4, 5};
  auto buf = ByteBuffer::wrapExternal(data, 5);

  CHECK(buf->isExternal());
  CHECK_FALSE(buf->isWritable());
  CHECK_EQ(buf->length(), 5);
  CHECK_EQ(buf->data(), data);  // Zero-copy: same pointer
}

TEST_CASE("ByteBuffer: wrapExternal null with zero size") {
  auto buf = ByteBuffer::wrapExternal(nullptr, 0);
  CHECK(buf->isExternal());
  CHECK_EQ(buf->length(), 0);
}

TEST_CASE("ByteBuffer: wrapExternal null with non-zero size throws") {
  CHECK_THROWS_AS(ByteBuffer::wrapExternal(nullptr, 10), std::invalid_argument);
}

// =============================================================================
// Data Access Tests
// =============================================================================

TEST_CASE("ByteBuffer: writableData") {
  auto buf = ByteBuffer::create(100);
  uint8_t* ptr = buf->writableData();
  CHECK_NE(ptr, nullptr);

  // Write some data
  std::memcpy(ptr, "test", 4);
  buf->append(4);

  CHECK_EQ(buf->length(), 4);
  CHECK_EQ(std::memcmp(buf->data(), "test", 4), 0);
}

TEST_CASE("ByteBuffer: writableData on external throws") {
  const uint8_t data[] = {1, 2, 3};
  auto buf = ByteBuffer::wrapExternal(data, 3);

  CHECK_THROWS_AS(buf->writableData(), std::logic_error);
}

TEST_CASE("ByteBuffer: writableDataOrNull") {
  auto ownedBuf = ByteBuffer::create(100);
  CHECK_NE(ownedBuf->writableDataOrNull(), nullptr);

  const uint8_t data[] = {1, 2, 3};
  auto externalBuf = ByteBuffer::wrapExternal(data, 3);
  CHECK_EQ(externalBuf->writableDataOrNull(), nullptr);
}

// =============================================================================
// Append/Prepend Tests
// =============================================================================

TEST_CASE("ByteBuffer: append") {
  auto buf = ByteBuffer::create(100);
  std::memcpy(buf->writableData(), "hello", 5);
  buf->append(5);

  CHECK_EQ(buf->length(), 5);
  CHECK_EQ(buf->toString(), "hello");
}

TEST_CASE("ByteBuffer: append multiple") {
  auto buf = ByteBuffer::create(100);

  std::memcpy(buf->writableData(), "hello", 5);
  buf->append(5);

  std::memcpy(buf->writableData() + buf->length(), " world", 6);
  buf->append(6);

  CHECK_EQ(buf->length(), 11);
  CHECK_EQ(buf->toString(), "hello world");
}

TEST_CASE("ByteBuffer: prepend with headroom") {
  auto buf = ByteBuffer::create(100);

  // Start with some data
  std::memcpy(buf->writableData(), "world", 5);
  buf->append(5);

  // Prepend should use headroom (O(1))
  size_t originalHeadroom = buf->headroom();
  if (originalHeadroom >= 6) {
    buf->prepend(6);
    std::memcpy(buf->writableData(), "hello ", 6);

    CHECK_EQ(buf->length(), 11);
    CHECK_EQ(buf->toString(), "hello world");
  }
}

TEST_CASE("ByteBuffer: prepend on external throws") {
  const uint8_t data[] = {1, 2, 3};
  auto buf = ByteBuffer::wrapExternal(data, 3);

  CHECK_THROWS_AS(buf->prepend(5), std::logic_error);
}

// =============================================================================
// TrimStart/TrimEnd Tests
// =============================================================================

TEST_CASE("ByteBuffer: trimStart") {
  auto buf = ByteBuffer::copyBuffer("hello world", 11);

  buf->trimStart(6);

  CHECK_EQ(buf->length(), 5);
  CHECK_EQ(buf->toString(), "world");
}

TEST_CASE("ByteBuffer: trimStart all") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);

  buf->trimStart(10);  // More than length

  CHECK_EQ(buf->length(), 0);
  CHECK(buf->empty());
}

TEST_CASE("ByteBuffer: trimStart O(1)") {
  // Verify trimStart is O(1) via offset adjustment
  auto buf = ByteBuffer::copyBuffer("hello world", 11);

  size_t originalHeadroom = buf->headroom();
  buf->trimStart(3);

  // Headroom should increase by trimmed amount
  CHECK_EQ(buf->headroom(), originalHeadroom + 3);
  CHECK_EQ(buf->toString(), "lo world");
}

TEST_CASE("ByteBuffer: trimEnd") {
  auto buf = ByteBuffer::copyBuffer("hello world", 11);

  buf->trimEnd(6);

  CHECK_EQ(buf->length(), 5);
  CHECK_EQ(buf->toString(), "hello");
}

TEST_CASE("ByteBuffer: trimEnd all") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);

  buf->trimEnd(10);  // More than length

  CHECK_EQ(buf->length(), 0);
  CHECK(buf->empty());
}

// =============================================================================
// Headroom/Tailroom Tests
// =============================================================================

TEST_CASE("ByteBuffer: headroom") {
  ByteBuffer buf(200);

  // Heap-allocated buffer should have default headroom
  CHECK_GT(buf.headroom(), 0);
}

TEST_CASE("ByteBuffer: tailroom") {
  ByteBuffer buf(200);

  std::memcpy(buf.writableData(), "test", 4);
  buf.append(4);

  CHECK_GT(buf.tailroom(), 0);
}

TEST_CASE("ByteBuffer: headroom after trim") {
  auto buf = ByteBuffer::copyBuffer("hello world", 11);

  size_t initialHeadroom = buf->headroom();
  buf->trimStart(5);

  // Headroom should increase
  CHECK_EQ(buf->headroom(), initialHeadroom + 5);
}

// =============================================================================
// Clone Tests
// =============================================================================

TEST_CASE("ByteBuffer: clone") {
  auto original = ByteBuffer::copyBuffer("test data", 9);
  auto cloned = original->clone();

  // Should be deep copy
  CHECK_NE(original->data(), cloned->data());
  CHECK_EQ(original->length(), cloned->length());
  CHECK_EQ(original->toString(), cloned->toString());
}

TEST_CASE("ByteBuffer: clone external") {
  const uint8_t data[] = {1, 2, 3, 4, 5};
  auto external = ByteBuffer::wrapExternal(data, 5);

  // Cloning external buffer should create owned copy
  auto cloned = external->clone();

  CHECK_FALSE(cloned->isExternal());
  CHECK(cloned->isWritable());
  CHECK_NE(cloned->data(), data);  // Different memory
  CHECK_EQ(cloned->length(), 5);
}

TEST_CASE("ByteBuffer: cloneIfExternal") {
  // Owned buffer: should return nullptr
  auto owned = ByteBuffer::copyBuffer("test", 4);
  CHECK_EQ(owned->cloneIfExternal(), nullptr);

  // External buffer: should return clone
  const uint8_t data[] = {1, 2, 3};
  auto external = ByteBuffer::wrapExternal(data, 3);
  auto cloned = external->cloneIfExternal();

  CHECK_NE(cloned, nullptr);
  CHECK_FALSE(cloned->isExternal());
}

// =============================================================================
// Reserve/EnsureWritableSpace Tests
// =============================================================================

TEST_CASE("ByteBuffer: reserve") {
  auto buf = ByteBuffer::create(100);
  size_t originalCapacity = buf->capacity();

  buf->reserve(originalCapacity + 500);

  CHECK_GE(buf->capacity(), originalCapacity + 500);
}

TEST_CASE("ByteBuffer: reserve no-op if enough") {
  auto buf = ByteBuffer::create(1000);
  size_t originalCapacity = buf->capacity();

  buf->reserve(500);  // Less than current

  // Should not change capacity
  CHECK_EQ(buf->capacity(), originalCapacity);
}

TEST_CASE("ByteBuffer: reserve on external throws") {
  const uint8_t data[] = {1, 2, 3};
  auto buf = ByteBuffer::wrapExternal(data, 3);

  CHECK_THROWS_AS(buf->reserve(100), std::logic_error);
}

TEST_CASE("ByteBuffer: ensureWritableSpace") {
  auto buf = ByteBuffer::create(50);
  buf->append(40);  // Use up some space

  size_t tailroom = buf->tailroom();
  if (tailroom < 100) {
    buf->ensureWritableSpace(100);
    CHECK_GE(buf->tailroom(), 100);
  }
}

// =============================================================================
// String Conversion Tests
// =============================================================================

TEST_CASE("ByteBuffer: toString") {
  auto buf = ByteBuffer::copyBuffer("hello world", 11);
  CHECK_EQ(buf->toString(), "hello world");
}

TEST_CASE("ByteBuffer: moveToString") {
  auto buf = ByteBuffer::copyBuffer("test string", 11);
  std::string str = buf->moveToString();
  CHECK_EQ(str, "test string");
}

TEST_CASE("ByteBuffer: moveToFbString") {
  auto buf = ByteBuffer::copyBuffer("fbstring test", 13);
  auto fbStr = buf->moveToFbString();
  CHECK_EQ(fbStr.toStdString(), "fbstring test");
}

// =============================================================================
// ChainLength Tests
// =============================================================================

TEST_CASE("ByteBuffer: computeChainDataLength") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);
  CHECK_EQ(buf->computeChainDataLength(), 5);
}

// =============================================================================
// Buffer Pool Tests
// =============================================================================

TEST_CASE("ByteBuffer: buffer pool allocation") {
  // Create and destroy multiple buffers to test pool
  std::vector<std::unique_ptr<ByteBuffer>> buffers;

  for (int i = 0; i < 20; ++i) {
    buffers.push_back(ByteBuffer::create(512));
  }

  // Clear all buffers (return to pool)
  buffers.clear();

  // Allocate again (should get from pool)
  for (int i = 0; i < 20; ++i) {
    auto buf = ByteBuffer::create(512);
    CHECK_NE(buf, nullptr);
  }
}

// =============================================================================
// Small Buffer Optimization Tests
// =============================================================================

TEST_CASE("ByteBuffer: SBO small buffer") {
  // Buffer <= 64 bytes should use inline storage
  ByteBuffer buf(32);

  std::memcpy(buf.writableData(), "small", 5);
  buf.append(5);

  CHECK_EQ(buf.toString(), "small");
}

TEST_CASE("ByteBuffer: SBO threshold") {
  // Test exactly at SBO threshold
  ByteBuffer small(ByteBuffer::kInlineSize);
  ByteBuffer large(ByteBuffer::kInlineSize + 1);

  // Both should work correctly
  CHECK_GE(small.capacity(), ByteBuffer::kInlineSize);
  CHECK_GE(large.capacity(), ByteBuffer::kInlineSize + 1);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("ByteBuffer: empty buffer") {
  ByteBuffer buf;

  CHECK(buf.empty());
  CHECK_EQ(buf.length(), 0);
  CHECK_EQ(buf.toString(), "");
}

TEST_CASE("ByteBuffer: zero length copy") {
  auto buf = ByteBuffer::copyBuffer("", 0);
  CHECK_EQ(buf->length(), 0);
  CHECK(buf->empty());
}

TEST_CASE("ByteBuffer: large buffer") {
  const size_t largeSize = 100 * 1024;  // 100KB
  auto buf = ByteBuffer::create(largeSize);

  std::vector<uint8_t> data(largeSize, 0xAB);
  std::memcpy(buf->writableData(), data.data(), largeSize);
  buf->append(largeSize);

  CHECK_EQ(buf->length(), largeSize);
  CHECK_EQ(buf->data()[0], 0xAB);
  CHECK_EQ(buf->data()[largeSize - 1], 0xAB);
}

} // namespace moxygen::compat::test

#endif // !MOXYGEN_USE_FOLLY
