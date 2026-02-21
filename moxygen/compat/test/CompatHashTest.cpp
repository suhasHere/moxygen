/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/Hash.h>

#include <doctest/doctest.h>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace moxygen::compat::test {

// =============================================================================
// hash_combine Tests
// =============================================================================

TEST_CASE("hash_combine: basic") {
  size_t h1 = hash_combine(0, size_t{42});
  size_t h2 = hash_combine(0, size_t{42});
  size_t h3 = hash_combine(0, size_t{99});

  // Same input should produce same output
  CHECK_EQ(h1, h2);

  // Different input should produce different output
  CHECK_NE(h1, h3);
}

TEST_CASE("hash_combine: variadic") {
  size_t h = hash_combine(0, 1, 2, 3, 4, 5);

  // Should produce a valid hash
  CHECK_NE(h, 0);
}

TEST_CASE("hash_combine: order matters") {
  // Order should matter
  size_t h1 = hash_combine(0, 1, 2, 3);
  size_t h2 = hash_combine(0, 3, 2, 1);

  CHECK_NE(h1, h2);
}

// =============================================================================
// hash_range Tests
// =============================================================================

TEST_CASE("hash_range: vector") {
  std::vector<int> v1 = {1, 2, 3, 4, 5};
  std::vector<int> v2 = {1, 2, 3, 4, 5};
  std::vector<int> v3 = {5, 4, 3, 2, 1};

  size_t h1 = hash_range(v1.begin(), v1.end());
  size_t h2 = hash_range(v2.begin(), v2.end());
  size_t h3 = hash_range(v3.begin(), v3.end());

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("hash_range: empty") {
  std::vector<int> empty;
  size_t h = hash_range(empty.begin(), empty.end());
  CHECK_EQ(h, 0);
}

// =============================================================================
// to_underlying Tests
// =============================================================================

TEST_CASE("to_underlying") {
  enum class Color : int { Red = 1, Green = 2, Blue = 3 };
  enum class Size : uint8_t { Small = 10, Medium = 20, Large = 30 };

  CHECK_EQ(to_underlying(Color::Red), 1);
  CHECK_EQ(to_underlying(Color::Green), 2);
  CHECK_EQ(to_underlying(Size::Small), 10);
  CHECK_EQ(to_underlying(Size::Large), 30);
}

// =============================================================================
// HashCombiner Tests
// =============================================================================

TEST_CASE("HashCombiner: basic") {
  HashCombiner combiner;

  combiner(1);
  combiner(2);
  combiner(3);

  size_t result = combiner.result();
  CHECK_NE(result, 0);
}

TEST_CASE("HashCombiner: deterministic") {
  HashCombiner c1, c2;

  c1(10);
  c1(20);

  c2(10);
  c2(20);

  CHECK_EQ(c1.result(), c2.result());
}

#if !MOXYGEN_USE_FOLLY

// =============================================================================
// SecureStringHash Tests (std-mode only)
// =============================================================================

TEST_CASE("SecureStringHash: basic") {
  SecureStringHash hasher;

  size_t h1 = hasher("hello");
  size_t h2 = hasher("hello");
  size_t h3 = hasher("world");

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("SecureStringHash: string_view") {
  SecureStringHash hasher;

  std::string_view sv = "test";
  std::string str = "test";

  CHECK_EQ(hasher(sv), hasher(str));
}

TEST_CASE("SecureStringHash: empty") {
  SecureStringHash hasher;

  size_t h1 = hasher("");
  size_t h2 = hasher(std::string{});

  CHECK_EQ(h1, h2);
}

// =============================================================================
// IntegerHash Tests
// =============================================================================

TEST_CASE("IntegerHash: basic") {
  IntegerHash hasher;

  // Test avalanche - sequential inputs should produce different hashes
  std::set<size_t> hashes;
  for (int i = 0; i < 1000; ++i) {
    hashes.insert(hasher(i));
  }

  // With good avalanche, most hashes should be unique
  CHECK_GT(hashes.size(), 990);
}

TEST_CASE("IntegerHash: different types") {
  IntegerHash hasher;

  size_t h8 = hasher(static_cast<uint8_t>(42));
  size_t h16 = hasher(static_cast<uint16_t>(42));
  size_t h32 = hasher(static_cast<uint32_t>(42));
  size_t h64 = hasher(static_cast<uint64_t>(42));

  // All should produce valid hashes (may or may not be equal)
  CHECK_NE(h8, 0);
  CHECK_NE(h16, 0);
  CHECK_NE(h32, 0);
  CHECK_NE(h64, 0);
}

// =============================================================================
// PointerHash Tests
// =============================================================================

TEST_CASE("PointerHash: basic") {
  PointerHash hasher;

  int a = 1, b = 2;
  size_t h1 = hasher(&a);
  size_t h2 = hasher(&a);
  size_t h3 = hasher(&b);

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("PointerHash: null") {
  PointerHash hasher;

  int* null = nullptr;
  size_t h = hasher(null);
  // Null pointer should hash to something (possibly 0)
  (void)h;  // Just verify it doesn't crash
}

// =============================================================================
// EnumHash Tests
// =============================================================================

TEST_CASE("EnumHash: basic") {
  enum class Status { OK, Error, Pending };

  EnumHash hasher;

  size_t h1 = hasher(Status::OK);
  size_t h2 = hasher(Status::Error);
  size_t h3 = hasher(Status::OK);

  CHECK_EQ(h1, h3);
  CHECK_NE(h1, h2);
}

// =============================================================================
// PairHash Tests
// =============================================================================

TEST_CASE("PairHash: basic") {
  PairHash hasher;

  auto p1 = std::make_pair(1, 2);
  auto p2 = std::make_pair(1, 2);
  auto p3 = std::make_pair(2, 1);

  CHECK_EQ(hasher(p1), hasher(p2));
  CHECK_NE(hasher(p1), hasher(p3));
}

TEST_CASE("PairHash: in map") {
  std::unordered_map<std::pair<int, int>, std::string, PairHash> map;

  map[{1, 2}] = "one-two";
  map[{3, 4}] = "three-four";

  CHECK_EQ(map[{1, 2}], "one-two");
  CHECK_EQ(map[{3, 4}], "three-four");
}

// =============================================================================
// TupleHash Tests
// =============================================================================

TEST_CASE("TupleHash: basic") {
  TupleHash hasher;

  auto t1 = std::make_tuple(1, 2, 3);
  auto t2 = std::make_tuple(1, 2, 3);
  auto t3 = std::make_tuple(3, 2, 1);

  CHECK_EQ(hasher(t1), hasher(t2));
  CHECK_NE(hasher(t1), hasher(t3));
}

TEST_CASE("TupleHash: mixed types") {
  TupleHash hasher;

  auto t = std::make_tuple(42, std::string("hello"), 3.14);
  size_t h = hasher(t);

  CHECK_NE(h, 0);
}

// =============================================================================
// TransparentStringHash Tests
// =============================================================================

TEST_CASE("TransparentStringHash: heterogeneous lookup") {
  std::unordered_map<
      std::string,
      int,
      TransparentStringHash,
      TransparentStringEqual> map;

  map["hello"] = 1;
  map["world"] = 2;

  // Should be able to lookup with string_view
  std::string_view sv = "hello";
  auto it = map.find(sv);
  CHECK_NE(it, map.end());
  CHECK_EQ(it->second, 1);
}

TEST_CASE("TransparentStringHash: consistency") {
  TransparentStringHash hasher;

  std::string str = "test";
  std::string_view sv = str;
  const char* cstr = "test";

  CHECK_EQ(hasher(str), hasher(sv));
  CHECK_EQ(hasher(sv), hasher(cstr));
}

// =============================================================================
// FnvHash Tests
// =============================================================================

TEST_CASE("FnvHash: basic") {
  FnvHash hasher;

  size_t h1 = hasher(std::string_view("hello"));
  size_t h2 = hasher(std::string_view("hello"));
  size_t h3 = hasher(std::string_view("world"));

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("FnvHash: raw bytes") {
  FnvHash hasher;

  uint8_t data[] = {1, 2, 3, 4, 5};
  size_t h1 = hasher(data, 5);
  size_t h2 = hasher(data, 5);
  size_t h3 = hasher(data, 4);  // Different length

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

// =============================================================================
// WyHash Tests
// =============================================================================

TEST_CASE("WyHash: basic") {
  WyHash hasher;

  size_t h1 = hasher(std::string_view("hello"));
  size_t h2 = hasher(std::string_view("hello"));
  size_t h3 = hasher(std::string_view("world"));

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("WyHash: integers") {
  WyHash hasher;

  size_t h1 = hasher(42);
  size_t h2 = hasher(42);
  size_t h3 = hasher(99);

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("WyHash: raw bytes") {
  WyHash hasher;

  uint8_t data[] = {1, 2, 3, 4, 5};
  size_t h = hasher(data, 5);

  CHECK_NE(h, 0);
}

// =============================================================================
// Constexpr Hash Tests
// =============================================================================

TEST_CASE("constexprHash: basic") {
  constexpr size_t h1 = constexprHash("hello");
  constexpr size_t h2 = constexprHash("hello");
  constexpr size_t h3 = constexprHash("world");

  static_assert(h1 == h2, "Same string should hash the same");
  static_assert(h1 != h3, "Different strings should hash differently");

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("hash literal") {
  constexpr size_t h = "hello"_hash;

  CHECK_EQ(h, constexprHash("hello"));
}

TEST_CASE("hash literal in switch") {
  auto testSwitch = [](std::string_view s) -> int {
    switch (constexprHash(s)) {
      case "hello"_hash:
        return 1;
      case "world"_hash:
        return 2;
      default:
        return 0;
    }
  };

  CHECK_EQ(testSwitch("hello"), 1);
  CHECK_EQ(testSwitch("world"), 2);
  CHECK_EQ(testSwitch("other"), 0);
}

#endif // !MOXYGEN_USE_FOLLY

// =============================================================================
// hashBytes Tests
// =============================================================================

TEST_CASE("hashBytes: basic") {
  const char* data1 = "hello";
  const char* data2 = "hello";
  const char* data3 = "world";

  size_t h1 = hashBytes(data1, 5);
  size_t h2 = hashBytes(data2, 5);
  size_t h3 = hashBytes(data3, 5);

  CHECK_EQ(h1, h2);
  CHECK_NE(h1, h3);
}

TEST_CASE("hashBytes: empty") {
  size_t h = hashBytes("", 0);
  // Empty data should produce a valid hash
  (void)h;
}

// =============================================================================
// Distribution Tests
// =============================================================================

TEST_CASE("Hash distribution") {
  // Test that hashes are well-distributed (no obvious patterns)
  std::set<size_t> hashes;

  for (int i = 0; i < 10000; ++i) {
    std::string s = "key_" + std::to_string(i);
    hashes.insert(hashBytes(s.data(), s.size()));
  }

  // With good distribution, collision rate should be very low
  // (10000 items should have very few collisions)
  CHECK_GT(hashes.size(), 9900);
}

} // namespace moxygen::compat::test
