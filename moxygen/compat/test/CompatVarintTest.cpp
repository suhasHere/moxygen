/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/Varint.h>
#include <moxygen/compat/ByteBufferQueue.h>
#include <moxygen/compat/ByteCursor.h>

#include <doctest/doctest.h>
#include <cstring>

#if !(MOXYGEN_USE_FOLLY && MOXYGEN_QUIC_MVFST)

namespace moxygen::compat::test {

// =============================================================================
// getQuicIntegerSize Tests
// =============================================================================

TEST_CASE("getQuicIntegerSize: 1 byte") {
  // Values 0-63 should use 1 byte
  auto result0 = quic::getQuicIntegerSize(0);
  CHECK(result0.hasValue());
  CHECK_EQ(*result0, 1);

  auto result63 = quic::getQuicIntegerSize(63);
  CHECK(result63.hasValue());
  CHECK_EQ(*result63, 1);
}

TEST_CASE("getQuicIntegerSize: 2 bytes") {
  // Values 64-16383 should use 2 bytes
  auto result64 = quic::getQuicIntegerSize(64);
  CHECK(result64.hasValue());
  CHECK_EQ(*result64, 2);

  auto result16383 = quic::getQuicIntegerSize(16383);
  CHECK(result16383.hasValue());
  CHECK_EQ(*result16383, 2);
}

TEST_CASE("getQuicIntegerSize: 4 bytes") {
  // Values 16384-1073741823 should use 4 bytes
  auto result16384 = quic::getQuicIntegerSize(16384);
  CHECK(result16384.hasValue());
  CHECK_EQ(*result16384, 4);

  auto result1073741823 = quic::getQuicIntegerSize(1073741823);
  CHECK(result1073741823.hasValue());
  CHECK_EQ(*result1073741823, 4);
}

TEST_CASE("getQuicIntegerSize: 8 bytes") {
  // Values 1073741824 to 2^62-1 should use 8 bytes
  auto result = quic::getQuicIntegerSize(1073741824);
  CHECK(result.hasValue());
  CHECK_EQ(*result, 8);

  auto resultMax = quic::getQuicIntegerSize(quic::kMaxQuicIntegerValue);
  CHECK(resultMax.hasValue());
  CHECK_EQ(*resultMax, 8);
}

TEST_CASE("getQuicIntegerSize: exceeds max") {
  // Values > 2^62-1 should return error
  auto result = quic::getQuicIntegerSize(quic::kMaxQuicIntegerValue + 1);
  CHECK(result.hasError());
  CHECK_EQ(result.error(), quic::TransportErrorCode::INTERNAL_ERROR);
}

// =============================================================================
// encodeQuicInteger Tests
// =============================================================================

TEST_CASE("encodeQuicInteger: zero") {
  uint8_t buf[8] = {0};
  size_t written = quic::encodeQuicInteger(0, buf, 8);

  CHECK_EQ(written, 1);
  CHECK_EQ(buf[0], 0x00);
}

TEST_CASE("encodeQuicInteger: 1 byte") {
  uint8_t buf[8] = {0};

  // Encode 37 (0x25) - should be 1 byte
  size_t written = quic::encodeQuicInteger(37, buf, 8);

  CHECK_EQ(written, 1);
  CHECK_EQ(buf[0], 0x25);
}

TEST_CASE("encodeQuicInteger: 2 bytes") {
  uint8_t buf[8] = {0};

  // Encode 15293 - should be 2 bytes
  size_t written = quic::encodeQuicInteger(15293, buf, 8);

  CHECK_EQ(written, 2);
  // Type prefix is 01 (2 bytes), value is 15293
  // 01 | (15293 >> 8) = 0x7b, 15293 & 0xff = 0xbd
  CHECK_EQ(buf[0], 0x7b);
  CHECK_EQ(buf[1], 0xbd);
}

TEST_CASE("encodeQuicInteger: 4 bytes") {
  uint8_t buf[8] = {0};

  // Encode 494878333 - should be 4 bytes
  size_t written = quic::encodeQuicInteger(494878333, buf, 8);

  CHECK_EQ(written, 4);
  // Type prefix is 10 (4 bytes)
  CHECK_EQ((buf[0] >> 6) & 0x3, 2);  // Verify type prefix
}

TEST_CASE("encodeQuicInteger: 8 bytes") {
  uint8_t buf[8] = {0};

  // Encode 151288809941952652 - should be 8 bytes
  size_t written = quic::encodeQuicInteger(151288809941952652ULL, buf, 8);

  CHECK_EQ(written, 8);
  // Type prefix is 11 (8 bytes)
  CHECK_EQ((buf[0] >> 6) & 0x3, 3);  // Verify type prefix
}

TEST_CASE("encodeQuicInteger: insufficient buffer") {
  uint8_t buf[1] = {0};

  // Try to encode value that needs 2 bytes into 1-byte buffer
  size_t written = quic::encodeQuicInteger(1000, buf, 1);

  CHECK_EQ(written, 0);  // Should fail
}

TEST_CASE("encodeQuicInteger: max value") {
  uint8_t buf[8] = {0};

  size_t written = quic::encodeQuicInteger(quic::kMaxQuicIntegerValue, buf, 8);

  CHECK_EQ(written, 8);
}

TEST_CASE("encodeQuicInteger: over max fails") {
  uint8_t buf[8] = {0};

  size_t written = quic::encodeQuicInteger(quic::kMaxQuicIntegerValue + 1, buf, 8);

  CHECK_EQ(written, 0);  // Should fail
}

// =============================================================================
// decodeQuicInteger (raw buffer) Tests
// =============================================================================

TEST_CASE("decodeQuicInteger: zero") {
  const uint8_t data[] = {0x00};
  const uint8_t* ptr = data;
  size_t len = 1;

  auto result = quic::decodeQuicInteger(ptr, len);

  CHECK(result.has_value());
  CHECK_EQ(*result, 0);
  CHECK_EQ(len, 0);
}

TEST_CASE("decodeQuicInteger: 1 byte") {
  const uint8_t data[] = {0x25};  // 37
  const uint8_t* ptr = data;
  size_t len = 1;

  auto result = quic::decodeQuicInteger(ptr, len);

  CHECK(result.has_value());
  CHECK_EQ(*result, 37);
  CHECK_EQ(len, 0);
}

TEST_CASE("decodeQuicInteger: 2 bytes") {
  const uint8_t data[] = {0x7b, 0xbd};  // 15293
  const uint8_t* ptr = data;
  size_t len = 2;

  auto result = quic::decodeQuicInteger(ptr, len);

  CHECK(result.has_value());
  CHECK_EQ(*result, 15293);
  CHECK_EQ(len, 0);
}

TEST_CASE("decodeQuicInteger: 4 bytes") {
  const uint8_t data[] = {0x9d, 0x7f, 0x3e, 0x7d};  // 494878333
  const uint8_t* ptr = data;
  size_t len = 4;

  auto result = quic::decodeQuicInteger(ptr, len);

  CHECK(result.has_value());
  CHECK_EQ(*result, 494878333);
  CHECK_EQ(len, 0);
}

TEST_CASE("decodeQuicInteger: 8 bytes") {
  const uint8_t data[] = {0xc2, 0x19, 0x7c, 0x5e, 0xff, 0x14, 0xe8, 0x8c};
  const uint8_t* ptr = data;
  size_t len = 8;

  auto result = quic::decodeQuicInteger(ptr, len);

  CHECK(result.has_value());
  CHECK_EQ(*result, 151288809941952652ULL);
  CHECK_EQ(len, 0);
}

TEST_CASE("decodeQuicInteger: insufficient buffer") {
  const uint8_t data[] = {0x7b};  // 2-byte encoding, but only 1 byte provided
  const uint8_t* ptr = data;
  size_t len = 1;

  auto result = quic::decodeQuicInteger(ptr, len);

  CHECK_FALSE(result.has_value());
}

TEST_CASE("decodeQuicInteger: empty buffer") {
  const uint8_t* ptr = nullptr;
  size_t len = 0;

  auto result = quic::decodeQuicInteger(ptr, len);

  CHECK_FALSE(result.has_value());
}

// =============================================================================
// Roundtrip Tests
// =============================================================================

TEST_CASE("varint roundtrip: all sizes") {
  std::vector<uint64_t> testValues = {
      0,                        // Min 1-byte
      63,                       // Max 1-byte
      64,                       // Min 2-byte
      16383,                    // Max 2-byte
      16384,                    // Min 4-byte
      1073741823,               // Max 4-byte
      1073741824,               // Min 8-byte
      quic::kMaxQuicIntegerValue  // Max 8-byte
  };

  for (uint64_t value : testValues) {
    uint8_t buf[8] = {0};
    size_t written = quic::encodeQuicInteger(value, buf, 8);
    CHECK_GT(written, 0);

    const uint8_t* ptr = buf;
    size_t len = written;
    auto decoded = quic::decodeQuicInteger(ptr, len);

    CHECK(decoded.has_value());
    CHECK_EQ(*decoded, value);
  }
}

// =============================================================================
// Cursor-based decodeQuicInteger Tests
// =============================================================================

#if !MOXYGEN_USE_FOLLY

TEST_CASE("cursor decodeQuicInteger: 1 byte") {
  moxygen::compat::ByteBufferQueue queue;
  uint8_t data[] = {0x25};  // 37
  queue.append(moxygen::compat::ByteBuffer::copyBuffer(data, 1));

  moxygen::compat::ByteCursor cursor(&queue);

  auto result = quic::follyutils::decodeQuicInteger(cursor);

  CHECK(result.has_value());
  CHECK_EQ(result->first, 37);
  CHECK_EQ(result->second, 1);
}

TEST_CASE("cursor decodeQuicInteger: 2 bytes") {
  moxygen::compat::ByteBufferQueue queue;
  uint8_t data[] = {0x7b, 0xbd};  // 15293
  queue.append(moxygen::compat::ByteBuffer::copyBuffer(data, 2));

  moxygen::compat::ByteCursor cursor(&queue);

  auto result = quic::follyutils::decodeQuicInteger(cursor);

  CHECK(result.has_value());
  CHECK_EQ(result->first, 15293);
  CHECK_EQ(result->second, 2);
}

TEST_CASE("cursor decodeQuicInteger: across buffers") {
  moxygen::compat::ByteBufferQueue queue;
  // Split 4-byte varint across two buffers
  uint8_t data1[] = {0x9d, 0x7f};
  uint8_t data2[] = {0x3e, 0x7d};
  queue.append(moxygen::compat::ByteBuffer::copyBuffer(data1, 2));
  queue.append(moxygen::compat::ByteBuffer::copyBuffer(data2, 2));

  moxygen::compat::ByteCursor cursor(&queue);

  auto result = quic::follyutils::decodeQuicInteger(cursor);

  CHECK(result.has_value());
  CHECK_EQ(result->first, 494878333);
  CHECK_EQ(result->second, 4);
}

TEST_CASE("cursor decodeQuicInteger: with length") {
  moxygen::compat::ByteBufferQueue queue;
  uint8_t data[] = {0x25, 0x42};  // 37, then 0x42
  queue.append(moxygen::compat::ByteBuffer::copyBuffer(data, 2));

  moxygen::compat::ByteCursor cursor(&queue);
  size_t length = 2;

  auto result = quic::follyutils::decodeQuicInteger(cursor, length);

  CHECK(result.has_value());
  CHECK_EQ(result->first, 37);
  CHECK_EQ(result->second, 1);
  // Note: length is NOT modified by decodeQuicInteger
}

TEST_CASE("cursor decodeQuicInteger: insufficient length") {
  moxygen::compat::ByteBufferQueue queue;
  uint8_t data[] = {0x7b, 0xbd};  // 2-byte varint
  queue.append(moxygen::compat::ByteBuffer::copyBuffer(data, 2));

  moxygen::compat::ByteCursor cursor(&queue);
  size_t length = 1;  // Claim only 1 byte available

  auto result = quic::follyutils::decodeQuicInteger(cursor, length);

  CHECK_FALSE(result.has_value());
}

TEST_CASE("cursor decodeQuicInteger: empty") {
  moxygen::compat::ByteBufferQueue queue;
  moxygen::compat::ByteCursor cursor(&queue);

  auto result = quic::follyutils::decodeQuicInteger(cursor);

  CHECK_FALSE(result.has_value());
}

TEST_CASE("cursor decodeQuicInteger: multiple") {
  moxygen::compat::ByteBufferQueue queue;
  // Encode multiple values
  uint8_t data[] = {0x25, 0x7b, 0xbd, 0x00};  // 37, 15293, 0
  queue.append(moxygen::compat::ByteBuffer::copyBuffer(data, 4));

  moxygen::compat::ByteCursor cursor(&queue);

  auto r1 = quic::follyutils::decodeQuicInteger(cursor);
  CHECK(r1.has_value());
  CHECK_EQ(r1->first, 37);

  auto r2 = quic::follyutils::decodeQuicInteger(cursor);
  CHECK(r2.has_value());
  CHECK_EQ(r2->first, 15293);

  auto r3 = quic::follyutils::decodeQuicInteger(cursor);
  CHECK(r3.has_value());
  CHECK_EQ(r3->first, 0);
}

#endif // !MOXYGEN_USE_FOLLY

// =============================================================================
// Boundary Value Tests
// =============================================================================

TEST_CASE("varint boundary values") {
  struct TestCase {
    uint64_t value;
    size_t expectedSize;
  };

  std::vector<TestCase> testCases = {
      {0, 1},
      {63, 1},
      {64, 2},
      {16383, 2},
      {16384, 4},
      {1073741823, 4},
      {1073741824, 8},
      {quic::kMaxQuicIntegerValue, 8},
  };

  for (const auto& tc : testCases) {
    auto sizeResult = quic::getQuicIntegerSize(tc.value);
    CHECK(sizeResult.hasValue());
    CHECK_EQ(*sizeResult, tc.expectedSize);
  }
}

} // namespace moxygen::compat::test

#endif // !(MOXYGEN_USE_FOLLY && MOXYGEN_QUIC_MVFST)
