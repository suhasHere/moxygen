# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.

if(NOT BUILD_TESTS)
    return()
endif()

# =============================================================================
# Compat module unit tests
# =============================================================================
# These tests verify the correctness of the compatibility layer abstractions
# that provide Folly-like APIs in std-mode builds.
#
# Tests cover:
# - Async primitives (Promise/SemiFuture)
# - Buffer system (ByteBuffer, ByteBufferQueue, ByteCursor)
# - Error handling (Expected, Cancellation)
# - Hash utilities
# - QUIC varint encoding
# - MoQ priority queue
# =============================================================================

# Test main file for doctest
set(COMPAT_TEST_MAIN
    CompatTestMain.cpp
)

# Common test sources that work in both Folly and std modes
set(COMPAT_TEST_COMMON_SOURCES
    CompatAsyncTest.cpp
    CompatExpectedTest.cpp
    CompatCancellationTest.cpp
    CompatHashTest.cpp
)

# Test sources that only work in std mode (no Folly)
set(COMPAT_TEST_STD_ONLY_SOURCES
    CompatByteBufferTest.cpp
    CompatByteBufferQueueTest.cpp
    CompatByteCursorTest.cpp
    CompatVarintTest.cpp
    CompatMoQPriorityQueueTest.cpp
)

# =============================================================================
# Unit Tests
# =============================================================================

if(MOXYGEN_USE_FOLLY)
    # Folly mode: only run tests that work with Folly types
    add_executable(CompatTests
        ${COMPAT_TEST_MAIN}
        ${COMPAT_TEST_COMMON_SOURCES}
    )

    target_include_directories(
        CompatTests PUBLIC
        $<BUILD_INTERFACE:${MOXYGEN_FBCODE_ROOT}>
    )

    target_compile_options(
        CompatTests PRIVATE
        ${_MOXYGEN_COMMON_COMPILE_OPTIONS}
    )

    target_link_libraries(
        CompatTests
        moxygen
        moqcompat
        Folly::folly
        doctest::doctest
        Threads::Threads
    )

    doctest_discover_tests(CompatTests)

else()
    # Std mode: run all tests including std-only implementations
    add_executable(CompatTests
        ${COMPAT_TEST_MAIN}
        ${COMPAT_TEST_COMMON_SOURCES}
        ${COMPAT_TEST_STD_ONLY_SOURCES}
    )

    target_include_directories(
        CompatTests PUBLIC
        $<BUILD_INTERFACE:${MOXYGEN_FBCODE_ROOT}>
    )

    target_compile_options(
        CompatTests PRIVATE
        ${_MOXYGEN_COMMON_COMPILE_OPTIONS}
    )

    target_link_libraries(
        CompatTests
        moxygen
        moqcompat
        doctest::doctest
        Threads::Threads
    )

    doctest_discover_tests(CompatTests)

endif()

# =============================================================================
# Benchmark Tests
# =============================================================================
# These tests measure performance of critical compat module operations.
# Run manually for performance analysis, not part of regular test suite.

add_executable(CompatBenchmark
    CompatTestMain.cpp
    CompatBenchmark.cpp
)

target_include_directories(
    CompatBenchmark PUBLIC
    $<BUILD_INTERFACE:${MOXYGEN_FBCODE_ROOT}>
)

target_compile_options(
    CompatBenchmark PRIVATE
    ${_MOXYGEN_COMMON_COMPILE_OPTIONS}
    -O3  # Enable optimizations for benchmark accuracy
)

if(MOXYGEN_USE_FOLLY)
    target_link_libraries(
        CompatBenchmark
        moxygen
        moqcompat
        Folly::folly
        doctest::doctest
        Threads::Threads
    )
else()
    target_link_libraries(
        CompatBenchmark
        moxygen
        moqcompat
        doctest::doctest
        Threads::Threads
    )
endif()

# Don't run benchmarks as part of ctest by default
# Use: ./CompatBenchmark to run benchmarks

message(STATUS "Building compat module tests (CompatTests, CompatBenchmark)")
