/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/ByteCursor.h>

#include <doctest/doctest.h>
#include <cstring>

#if !MOXYGEN_USE_FOLLY

namespace moxygen::compat::test {

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("ByteCursor: construct from queue") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.totalLength(), 5);
  CHECK_FALSE(cursor.isAtEnd());
}

TEST_CASE("ByteCursor: construct from buffer") {
  auto buf = ByteBuffer::copyBuffer("test", 4);

  ByteCursor cursor(buf.get());

  CHECK_EQ(cursor.totalLength(), 4);
}

TEST_CASE("ByteCursor: construct from null queue") {
  ByteCursor cursor(static_cast<const ByteBufferQueue*>(nullptr));
  CHECK_EQ(cursor.totalLength(), 0);
  CHECK(cursor.isAtEnd());
}

TEST_CASE("ByteCursor: construct from null buffer") {
  ByteCursor cursor(static_cast<const ByteBuffer*>(nullptr));
  CHECK_EQ(cursor.totalLength(), 0);
  CHECK(cursor.isAtEnd());
}

TEST_CASE("ByteCursor: copy constructor") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor1(&queue);
  cursor1.skip(2);

  ByteCursor cursor2(cursor1);

  CHECK_EQ(cursor2.totalLength(), 3);
}

// =============================================================================
// Read Tests
// =============================================================================

TEST_CASE("ByteCursor: read uint8") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x42};
  queue.append(ByteBuffer::copyBuffer(data, 1));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.read<uint8_t>(), 0x42);
  CHECK(cursor.isAtEnd());
}

TEST_CASE("ByteCursor: read uint16 native") {
  ByteBufferQueue queue;
  uint16_t val = 0x1234;
  queue.append(ByteBuffer::copyBuffer(&val, sizeof(val)));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.read<uint16_t>(), 0x1234);
}

TEST_CASE("ByteCursor: readBE16") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x12, 0x34};  // Big-endian 0x1234
  queue.append(ByteBuffer::copyBuffer(data, 2));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.readBE<uint16_t>(), 0x1234);
}

TEST_CASE("ByteCursor: readBE32") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x12, 0x34, 0x56, 0x78};  // Big-endian
  queue.append(ByteBuffer::copyBuffer(data, 4));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.readBE<uint32_t>(), 0x12345678);
}

TEST_CASE("ByteCursor: readBE64") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  queue.append(ByteBuffer::copyBuffer(data, 8));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.readBE<uint64_t>(), 0x0102030405060708ULL);
}

TEST_CASE("ByteCursor: readLE16") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x34, 0x12};  // Little-endian 0x1234
  queue.append(ByteBuffer::copyBuffer(data, 2));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.readLE<uint16_t>(), 0x1234);
}

TEST_CASE("ByteCursor: readLE32") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x78, 0x56, 0x34, 0x12};  // Little-endian
  queue.append(ByteBuffer::copyBuffer(data, 4));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.readLE<uint32_t>(), 0x12345678);
}

TEST_CASE("ByteCursor: tryRead success") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x42};
  queue.append(ByteBuffer::copyBuffer(data, 1));

  ByteCursor cursor(&queue);

  auto result = cursor.tryRead<uint8_t>();
  CHECK(result.has_value());
  CHECK_EQ(*result, 0x42);
}

TEST_CASE("ByteCursor: tryRead failure") {
  ByteBufferQueue queue;
  uint8_t data[] = {0x42};
  queue.append(ByteBuffer::copyBuffer(data, 1));

  ByteCursor cursor(&queue);

  auto result = cursor.tryRead<uint32_t>();  // Not enough data
  CHECK_FALSE(result.has_value());
}

// =============================================================================
// Cross-Buffer Read Tests
// =============================================================================

TEST_CASE("ByteCursor: read across buffers") {
  ByteBufferQueue queue;
  uint8_t data1[] = {0x12, 0x34};
  uint8_t data2[] = {0x56, 0x78};
  queue.append(ByteBuffer::copyBuffer(data1, 2));
  queue.append(ByteBuffer::copyBuffer(data2, 2));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.readBE<uint32_t>(), 0x12345678);
}

// =============================================================================
// Skip Tests
// =============================================================================

TEST_CASE("ByteCursor: skip") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  cursor.skip(2);
  CHECK_EQ(cursor.totalLength(), 3);
  CHECK_EQ(cursor.read<uint8_t>(), 'l');
}

TEST_CASE("ByteCursor: skip all") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  cursor.skip(5);
  CHECK(cursor.isAtEnd());
}

TEST_CASE("ByteCursor: skip past end throws") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test", 4));

  ByteCursor cursor(&queue);

  CHECK_THROWS_AS(cursor.skip(10), CursorUnderflowError);
}

TEST_CASE("ByteCursor: trySkip success") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  CHECK(cursor.trySkip(3));
  CHECK_EQ(cursor.totalLength(), 2);
}

TEST_CASE("ByteCursor: trySkip failure") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hi", 2));

  ByteCursor cursor(&queue);

  CHECK_FALSE(cursor.trySkip(10));
  CHECK_EQ(cursor.totalLength(), 2);  // Unchanged
}

TEST_CASE("ByteCursor: skipAtEnd") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  cursor.skipAtEnd();
  CHECK(cursor.isAtEnd());
}

// =============================================================================
// Pull Tests
// =============================================================================

TEST_CASE("ByteCursor: pull") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  char buf[5];
  cursor.pull(buf, 5);

  CHECK_EQ(std::memcmp(buf, "hello", 5), 0);
  CHECK(cursor.isAtEnd());
}

TEST_CASE("ByteCursor: pull across buffers") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  ByteCursor cursor(&queue);

  char buf[11];
  cursor.pull(buf, 11);

  CHECK_EQ(std::memcmp(buf, "hello world", 11), 0);
}

TEST_CASE("ByteCursor: pull past end throws") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hi", 2));

  ByteCursor cursor(&queue);

  char buf[10];
  CHECK_THROWS_AS(cursor.pull(buf, 10), CursorUnderflowError);
}

TEST_CASE("ByteCursor: pullAtMost") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  char buf[10];
  size_t pulled = cursor.pullAtMost(buf, 10);

  CHECK_EQ(pulled, 5);
  CHECK_EQ(std::memcmp(buf, "hello", 5), 0);
}

// =============================================================================
// Clone Tests
// =============================================================================

TEST_CASE("ByteCursor: clone") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  ByteCursor cursor(&queue);

  auto cloned = cursor.clone(5);

  CHECK_EQ(cloned->length(), 5);
  CHECK_EQ(cloned->toString(), "hello");
  CHECK_EQ(cursor.totalLength(), 6);  // Remaining
}

TEST_CASE("ByteCursor: clone past end throws") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hi", 2));

  ByteCursor cursor(&queue);

  CHECK_THROWS_AS(cursor.clone(10), CursorUnderflowError);
}

TEST_CASE("ByteCursor: cloneAtMost") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hi", 2));

  ByteCursor cursor(&queue);

  auto cloned = cursor.cloneAtMost(10);

  CHECK_NE(cloned, nullptr);
  CHECK_EQ(cloned->length(), 2);
}

// =============================================================================
// Peek Tests
// =============================================================================

TEST_CASE("ByteCursor: peek") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  CHECK_EQ(cursor.peek(), 'h');
  CHECK_EQ(cursor.totalLength(), 5);  // Not consumed
}

TEST_CASE("ByteCursor: peek at end throws") {
  ByteBufferQueue queue;
  ByteCursor cursor(&queue);

  CHECK_THROWS_AS(cursor.peek(), CursorUnderflowError);
}

TEST_CASE("ByteCursor: tryPeek") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hi", 2));

  ByteCursor cursor(&queue);

  auto result = cursor.tryPeek();
  CHECK(result.has_value());
  CHECK_EQ(*result, 'h');
}

TEST_CASE("ByteCursor: tryPeek empty") {
  ByteBufferQueue queue;
  ByteCursor cursor(&queue);

  auto result = cursor.tryPeek();
  CHECK_FALSE(result.has_value());
}

TEST_CASE("ByteCursor: peekBytes") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  auto range = cursor.peekBytes();
  CHECK_EQ(range.size(), 5);
  CHECK_EQ(range[0], 'h');
}

TEST_CASE("ByteCursor: peekBytes with limit") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  auto range = cursor.peekBytes(3);
  CHECK_EQ(range.size(), 3);
}

// =============================================================================
// String Reading Tests
// =============================================================================

TEST_CASE("ByteCursor: readFixedString") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  ByteCursor cursor(&queue);

  std::string str = cursor.readFixedString(5);
  CHECK_EQ(str, "hello");
  CHECK_EQ(cursor.totalLength(), 6);
}

TEST_CASE("ByteCursor: tryReadFixedString success") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  auto result = cursor.tryReadFixedString(5);
  CHECK(result.has_value());
  CHECK_EQ(*result, "hello");
}

TEST_CASE("ByteCursor: tryReadFixedString failure") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hi", 2));

  ByteCursor cursor(&queue);

  auto result = cursor.tryReadFixedString(10);
  CHECK_FALSE(result.has_value());
}

TEST_CASE("ByteCursor: readTerminatedString") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello\0world", 11));

  ByteCursor cursor(&queue);

  std::string str = cursor.readTerminatedString('\0');
  CHECK_EQ(str, "hello");
  CHECK_EQ(cursor.totalLength(), 5);  // "world" remaining
}

TEST_CASE("ByteCursor: readNullTerminatedString") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("test\0", 5));

  ByteCursor cursor(&queue);

  std::string str = cursor.readNullTerminatedString();
  CHECK_EQ(str, "test");
}

TEST_CASE("ByteCursor: tryReadTerminatedString success") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello\0", 6));

  ByteCursor cursor(&queue);

  auto result = cursor.tryReadTerminatedString('\0');
  CHECK(result.has_value());
  CHECK_EQ(*result, "hello");
}

TEST_CASE("ByteCursor: tryReadTerminatedString no terminator") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  auto result = cursor.tryReadTerminatedString('\0');
  CHECK_FALSE(result.has_value());
  // Cursor should be rolled back
  CHECK_EQ(cursor.totalLength(), 5);
}

TEST_CASE("ByteCursor: readWhileNot") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  ByteCursor cursor(&queue);

  std::string str = cursor.readWhileNot(' ');
  CHECK_EQ(str, "hello");
  CHECK_EQ(cursor.peek(), ' ');
}

// =============================================================================
// Retreat Tests
// =============================================================================

TEST_CASE("ByteCursor: retreat single buffer") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);
  ByteCursor cursor(buf.get());

  cursor.skip(3);
  CHECK_EQ(cursor.totalLength(), 2);

  cursor.retreat(2);
  CHECK_EQ(cursor.totalLength(), 4);
  CHECK_EQ(cursor.peek(), 'e');
}

TEST_CASE("ByteCursor: retreat past beginning throws") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);
  ByteCursor cursor(buf.get());

  cursor.skip(2);
  CHECK_THROWS_AS(cursor.retreat(5), CursorOverflowError);
}

TEST_CASE("ByteCursor: retreat cross buffer") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));
  queue.append(ByteBuffer::copyBuffer(" world", 6));

  ByteCursor cursor(&queue);

  cursor.skip(7);  // Skip into second buffer
  CHECK_EQ(cursor.totalLength(), 4);

  cursor.retreat(4);  // Retreat back into first buffer
  CHECK_EQ(cursor.totalLength(), 8);
}

// =============================================================================
// CanAdvance Tests
// =============================================================================

TEST_CASE("ByteCursor: canAdvance") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);

  CHECK(cursor.canAdvance(5));
  CHECK(cursor.canAdvance(3));
  CHECK_FALSE(cursor.canAdvance(10));
}

// =============================================================================
// Cursor Difference Tests
// =============================================================================

TEST_CASE("ByteCursor: cursor difference") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  ByteCursor cursor1(&queue);
  ByteCursor cursor2(cursor1);

  cursor2.skip(5);

  CHECK_EQ(cursor2 - cursor1, 5);
}

// =============================================================================
// ByteRange Tests
// =============================================================================

TEST_CASE("ByteRange: basic") {
  const uint8_t data[] = {1, 2, 3, 4, 5};
  ByteRange range(data, 5);

  CHECK_EQ(range.size(), 5);
  CHECK_EQ(range[0], 1);
  CHECK_EQ(range[4], 5);
  CHECK_FALSE(range.empty());
}

TEST_CASE("ByteRange: empty") {
  ByteRange range;
  CHECK(range.empty());
  CHECK_EQ(range.size(), 0);
}

TEST_CASE("ByteRange: subpiece") {
  const uint8_t data[] = {1, 2, 3, 4, 5};
  ByteRange range(data, 5);

  auto sub = range.subpiece(1, 3);
  CHECK_EQ(sub.size(), 3);
  CHECK_EQ(sub[0], 2);
}

TEST_CASE("ByteRange: toString") {
  const uint8_t data[] = {'h', 'e', 'l', 'l', 'o'};
  ByteRange range(data, 5);

  CHECK_EQ(range.toString(), "hello");
}

// =============================================================================
// BoundedCursor Tests
// =============================================================================

TEST_CASE("BoundedCursor: basic") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello world", 11));

  ByteCursor cursor(&queue);
  BoundedCursor bounded(cursor, 5);

  CHECK_EQ(bounded.totalLength(), 5);
  CHECK_EQ(bounded.read<uint8_t>(), 'h');
  CHECK_EQ(bounded.totalLength(), 4);
}

TEST_CASE("BoundedCursor: read past bounds throws") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);
  BoundedCursor bounded(cursor, 3);

  bounded.skip(2);
  CHECK_THROWS_AS(bounded.read<uint32_t>(), CursorBoundsError);
}

TEST_CASE("BoundedCursor: tryRead") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);
  BoundedCursor bounded(cursor, 2);

  auto result = bounded.tryRead<uint32_t>();
  CHECK_FALSE(result.has_value());
}

TEST_CASE("BoundedCursor: skip past bounds throws") {
  ByteBufferQueue queue;
  queue.append(ByteBuffer::copyBuffer("hello", 5));

  ByteCursor cursor(&queue);
  BoundedCursor bounded(cursor, 3);

  CHECK_THROWS_AS(bounded.skip(5), CursorBoundsError);
}

// =============================================================================
// RWCursor Tests
// =============================================================================

TEST_CASE("RWCursor: read") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);
  RWCursor cursor(buf.get());

  CHECK_EQ(cursor.read<uint8_t>(), 'h');
  CHECK_EQ(cursor.totalLength(), 4);
}

TEST_CASE("RWCursor: write") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);
  RWCursor cursor(buf.get());

  cursor.write<uint8_t>('H');

  CHECK_EQ(buf->data()[0], 'H');
}

TEST_CASE("RWCursor: writeBE") {
  auto buf = ByteBuffer::create(4);
  buf->append(4);

  RWCursor cursor(buf.get());
  cursor.writeBE<uint32_t>(0x12345678);

  CHECK_EQ(buf->data()[0], 0x12);
  CHECK_EQ(buf->data()[1], 0x34);
  CHECK_EQ(buf->data()[2], 0x56);
  CHECK_EQ(buf->data()[3], 0x78);
}

TEST_CASE("RWCursor: writeLE") {
  auto buf = ByteBuffer::create(4);
  buf->append(4);

  RWCursor cursor(buf.get());
  cursor.writeLE<uint32_t>(0x12345678);

  CHECK_EQ(buf->data()[0], 0x78);
  CHECK_EQ(buf->data()[1], 0x56);
  CHECK_EQ(buf->data()[2], 0x34);
  CHECK_EQ(buf->data()[3], 0x12);
}

TEST_CASE("RWCursor: seek") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);
  RWCursor cursor(buf.get());

  cursor.seek(3);
  CHECK_EQ(cursor.position(), 3);
  CHECK_EQ(cursor.read<uint8_t>(), 'l');
}

TEST_CASE("RWCursor: reset") {
  auto buf = ByteBuffer::copyBuffer("hello", 5);
  RWCursor cursor(buf.get());

  cursor.skip(3);
  cursor.reset();

  CHECK_EQ(cursor.position(), 0);
  CHECK_EQ(cursor.totalLength(), 5);
}

// =============================================================================
// ByteAppender Tests
// =============================================================================

TEST_CASE("ByteAppender: write") {
  ByteBufferQueue queue;
  ByteAppender appender(&queue);

  appender.write("hello", 5);

  CHECK_EQ(queue.chainLength(), 5);
}

TEST_CASE("ByteAppender: writeBE") {
  ByteBufferQueue queue;
  ByteAppender appender(&queue);

  appender.writeBE<uint32_t>(0x12345678);

  ByteCursor cursor(&queue);
  CHECK_EQ(cursor.readBE<uint32_t>(), 0x12345678);
}

TEST_CASE("ByteAppender: writeLE") {
  ByteBufferQueue queue;
  ByteAppender appender(&queue);

  appender.writeLE<uint32_t>(0x12345678);

  ByteCursor cursor(&queue);
  CHECK_EQ(cursor.readLE<uint32_t>(), 0x12345678);
}

} // namespace moxygen::compat::test

#endif // !MOXYGEN_USE_FOLLY
