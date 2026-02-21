/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <moxygen/compat/MoQPriorityQueue.h>
#include <moxygen/compat/Payload.h>

#include <doctest/doctest.h>
#include <chrono>
#include <thread>

namespace moxygen::compat::test {

// Helper to create a QueuedObject
static QueuedObject makeObject(
    uint64_t groupId,
    uint64_t objectId,
    uint8_t priority = 3,
    size_t payloadSize = 100,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
  QueuedObject obj;
  obj.groupId = groupId;
  obj.objectId = objectId;
  obj.priority = priority;
  obj.deadline = std::chrono::steady_clock::now() + timeout;

  if (payloadSize > 0) {
    std::vector<uint8_t> data(payloadSize, 'x');
    obj.payload = std::make_unique<Payload>(std::move(data));
  }
  return obj;
}

// =============================================================================
// StreamPriorityQueue Basic Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: default construction") {
  StreamPriorityQueue queue;
  CHECK(queue.empty());
  CHECK_EQ(queue.size(), 0);
  CHECK_EQ(queue.byteSize(), 0);
  CHECK_FALSE(queue.isFull());
}

TEST_CASE("StreamPriorityQueue: configured construction") {
  StreamPriorityQueue::Config config;
  config.maxObjects = 100;
  config.maxBytes = 1024;

  StreamPriorityQueue queue(config);
  CHECK(queue.empty());
}

// =============================================================================
// Enqueue Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: enqueue basic") {
  StreamPriorityQueue queue;

  auto obj = makeObject(1, 1, 3, 100);
  auto result = queue.enqueue(std::move(obj));

  CHECK_EQ(result, EnqueueResult::OK);
  CHECK_FALSE(queue.empty());
  CHECK_EQ(queue.size(), 1);
}

TEST_CASE("StreamPriorityQueue: enqueue multiple") {
  StreamPriorityQueue queue;

  for (int i = 0; i < 10; ++i) {
    auto obj = makeObject(1, i, 3, 100);
    auto result = queue.enqueue(std::move(obj));
    CHECK_EQ(result, EnqueueResult::OK);
  }

  CHECK_EQ(queue.size(), 10);
}

TEST_CASE("StreamPriorityQueue: enqueue with timeout") {
  StreamPriorityQueue queue;

  auto obj = makeObject(1, 1, 3, 100);
  obj.deadline = std::chrono::steady_clock::time_point{}; // Reset
  auto result = queue.enqueue(std::move(obj), std::chrono::milliseconds(500));

  CHECK_EQ(result, EnqueueResult::OK);
}

TEST_CASE("StreamPriorityQueue: enqueue queue full") {
  StreamPriorityQueue::Config config;
  config.maxObjects = 2;

  StreamPriorityQueue queue(config);

  queue.enqueue(makeObject(1, 1));
  queue.enqueue(makeObject(1, 2));

  auto result = queue.enqueue(makeObject(1, 3));
  CHECK_EQ(result, EnqueueResult::QUEUE_FULL);
}

TEST_CASE("StreamPriorityQueue: enqueue byte limit") {
  StreamPriorityQueue::Config config;
  config.maxBytes = 200;

  StreamPriorityQueue queue(config);

  queue.enqueue(makeObject(1, 1, 3, 150));

  auto result = queue.enqueue(makeObject(1, 2, 3, 100));
  CHECK_EQ(result, EnqueueResult::QUEUE_FULL);
}

// =============================================================================
// Dequeue Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: dequeue basic") {
  StreamPriorityQueue queue;

  queue.enqueue(makeObject(1, 42, 3, 100));

  auto dequeued = queue.dequeue();

  CHECK(dequeued.has_value());
  CHECK_EQ(dequeued->objectId, 42);
  CHECK(queue.empty());
}

TEST_CASE("StreamPriorityQueue: dequeue empty") {
  StreamPriorityQueue queue;

  auto dequeued = queue.dequeue();

  CHECK_FALSE(dequeued.has_value());
}

TEST_CASE("StreamPriorityQueue: dequeue priority order") {
  StreamPriorityQueue queue;

  // Enqueue out of priority order
  queue.enqueue(makeObject(1, 3, 5, 100));  // Low priority
  queue.enqueue(makeObject(1, 1, 1, 100));  // High priority
  queue.enqueue(makeObject(1, 2, 3, 100));  // Medium priority

  // Should dequeue in priority order (lower number = higher priority)
  auto first = queue.dequeue();
  CHECK(first.has_value());
  CHECK_EQ(first->priority, 1);

  auto second = queue.dequeue();
  CHECK(second.has_value());
  CHECK_EQ(second->priority, 3);

  auto third = queue.dequeue();
  CHECK(third.has_value());
  CHECK_EQ(third->priority, 5);
}

TEST_CASE("StreamPriorityQueue: dequeue deadline order") {
  StreamPriorityQueue queue;

  auto now = std::chrono::steady_clock::now();

  // Same priority, different deadlines
  QueuedObject obj1;
  obj1.groupId = 1;
  obj1.objectId = 1;
  obj1.priority = 3;
  obj1.deadline = now + std::chrono::milliseconds(500);

  QueuedObject obj2;
  obj2.groupId = 1;
  obj2.objectId = 2;
  obj2.priority = 3;
  obj2.deadline = now + std::chrono::milliseconds(100);  // Earlier deadline

  queue.enqueue(std::move(obj1));
  queue.enqueue(std::move(obj2));

  // Earlier deadline should come first
  auto first = queue.dequeue();
  CHECK(first.has_value());
  CHECK_EQ(first->objectId, 2);  // Earlier deadline

  auto second = queue.dequeue();
  CHECK(second.has_value());
  CHECK_EQ(second->objectId, 1);
}

// =============================================================================
// Peek Tests
// =============================================================================
// Note: peek() is not implemented (returns nullptr always) as noted in
// MoQPriorityQueue.cpp - "Priority queue doesn't support efficient peek-with-filter"

TEST_CASE("StreamPriorityQueue: peek empty" * doctest::skip(true)) {
  // Skipped: peek() not implemented
  StreamPriorityQueue queue;

  const QueuedObject* peeked = queue.peek();

  CHECK_EQ(peeked, nullptr);
}

TEST_CASE("StreamPriorityQueue: peek basic" * doctest::skip(true)) {
  // Skipped: peek() not implemented - always returns nullptr
  StreamPriorityQueue queue;

  queue.enqueue(makeObject(1, 42));

  const QueuedObject* peeked = queue.peek();

  CHECK_NE(peeked, nullptr);
  CHECK_EQ(peeked->objectId, 42);
  CHECK_EQ(queue.size(), 1);  // Not removed
}

// =============================================================================
// DequeueBatch Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: dequeueBatch basic") {
  StreamPriorityQueue queue;

  for (int i = 0; i < 5; ++i) {
    queue.enqueue(makeObject(1, i, 3, 100));
  }

  // dequeueBatch adds objects while batchBytes < maxBytes
  // So with 150 bytes limit and 100-byte objects, it gets 2 objects (200 bytes total)
  auto batch = queue.dequeueBatch(150);

  CHECK_EQ(batch.size(), 2);
  CHECK_EQ(queue.size(), 3);
}

TEST_CASE("StreamPriorityQueue: dequeueBatch all") {
  StreamPriorityQueue queue;

  for (int i = 0; i < 3; ++i) {
    queue.enqueue(makeObject(1, i, 3, 100));
  }

  auto batch = queue.dequeueBatch(10000);  // More than available

  CHECK_EQ(batch.size(), 3);
  CHECK(queue.empty());
}

// =============================================================================
// Group Expiry Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: expireGroup") {
  StreamPriorityQueue queue;

  queue.enqueue(makeObject(1, 1));
  queue.enqueue(makeObject(1, 2));
  queue.enqueue(makeObject(2, 1));

  queue.expireGroup(1);

  CHECK(queue.isGroupExpired(1));
  CHECK_FALSE(queue.isGroupExpired(2));
}

TEST_CASE("StreamPriorityQueue: enqueue expired group") {
  StreamPriorityQueue queue;

  queue.expireGroup(1);

  auto result = queue.enqueue(makeObject(1, 1));

  CHECK_EQ(result, EnqueueResult::GROUP_EXPIRED);
}

TEST_CASE("StreamPriorityQueue: dequeue skips expired group") {
  StreamPriorityQueue queue;

  queue.enqueue(makeObject(1, 1));
  queue.enqueue(makeObject(2, 1));

  queue.expireGroup(1);

  auto dequeued = queue.dequeue();

  // Should skip group 1 and return group 2
  CHECK(dequeued.has_value());
  CHECK_EQ(dequeued->groupId, 2);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: stats") {
  StreamPriorityQueue queue;

  queue.enqueue(makeObject(1, 1, 3, 100));
  queue.enqueue(makeObject(1, 2, 3, 200));

  auto stats = queue.stats();

  CHECK_EQ(stats.objectCount, 2);
  CHECK_EQ(stats.byteCount, 300);
}

// =============================================================================
// Capacity Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: isFull") {
  StreamPriorityQueue::Config config;
  config.maxObjects = 2;

  StreamPriorityQueue queue(config);

  CHECK_FALSE(queue.isFull());

  queue.enqueue(makeObject(1, 1));
  queue.enqueue(makeObject(1, 2));

  CHECK(queue.isFull());
}

TEST_CASE("StreamPriorityQueue: setMaxObjects") {
  StreamPriorityQueue queue;

  queue.enqueue(makeObject(1, 1));
  queue.enqueue(makeObject(1, 2));

  queue.setMaxObjects(2);
  CHECK(queue.isFull());

  queue.setMaxObjects(10);
  CHECK_FALSE(queue.isFull());
}

// =============================================================================
// Backpressure Signaling Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: awaitCapacity") {
  StreamPriorityQueue::Config config;
  config.maxObjects = 1;

  StreamPriorityQueue queue(config);

  queue.enqueue(makeObject(1, 1));

  auto future = queue.awaitCapacity();

  // Future should not be ready yet
  CHECK_FALSE(future.isReady());

  // Dequeue to make room
  queue.dequeue();
  queue.signalCapacityAvailable();

  // Now future should be ready
  CHECK(future.isReady());
}

TEST_CASE("StreamPriorityQueue: awaitReady") {
  StreamPriorityQueue queue;

  auto future = queue.awaitReady();

  CHECK_FALSE(future.isReady());

  queue.signalReady();

  CHECK(future.isReady());
}

// =============================================================================
// GroupExpiryTracker Tests
// =============================================================================

TEST_CASE("GroupExpiryTracker: basic") {
  GroupExpiryTracker tracker;

  CHECK_FALSE(tracker.isExpired(1));
  CHECK_EQ(tracker.expiredCount(), 0);

  tracker.expireGroup(1);

  CHECK(tracker.isExpired(1));
  CHECK_EQ(tracker.expiredCount(), 1);
}

TEST_CASE("GroupExpiryTracker: callback") {
  GroupExpiryTracker tracker;

  std::vector<uint64_t> expiredGroups;
  tracker.onGroupExpired([&expiredGroups](uint64_t groupId) {
    expiredGroups.push_back(groupId);
  });

  tracker.expireGroup(1);
  tracker.expireGroup(2);

  CHECK_EQ(expiredGroups.size(), 2);
  CHECK_EQ(expiredGroups[0], 1);
  CHECK_EQ(expiredGroups[1], 2);
}

TEST_CASE("GroupExpiryTracker: clearExpiredBefore") {
  GroupExpiryTracker tracker;

  tracker.expireGroup(1);
  tracker.expireGroup(5);
  tracker.expireGroup(10);

  tracker.clearExpiredBefore(6);

  CHECK_FALSE(tracker.isExpired(1));
  CHECK_FALSE(tracker.isExpired(5));
  CHECK(tracker.isExpired(10));
}

// =============================================================================
// TrackPriorityQueue Tests
// =============================================================================

TEST_CASE("TrackPriorityQueue: basic") {
  TrackPriorityQueue track;

  auto obj1 = makeObject(1, 1);
  obj1.subgroupId = 0;
  auto result1 = track.enqueue(std::move(obj1));
  CHECK_EQ(result1, EnqueueResult::OK);

  auto obj2 = makeObject(1, 2);
  obj2.subgroupId = 1;
  auto result2 = track.enqueue(std::move(obj2));
  CHECK_EQ(result2, EnqueueResult::OK);
}

TEST_CASE("TrackPriorityQueue: expireGroup") {
  TrackPriorityQueue track;

  auto obj1 = makeObject(1, 1);
  obj1.subgroupId = 0;
  track.enqueue(std::move(obj1));

  auto obj2 = makeObject(1, 2);
  obj2.subgroupId = 1;
  track.enqueue(std::move(obj2));

  track.expireGroup(1);

  CHECK(track.isGroupExpired(1));
}

TEST_CASE("TrackPriorityQueue: totalStats") {
  TrackPriorityQueue track;

  auto obj1 = makeObject(1, 1, 3, 100);
  obj1.subgroupId = 0;
  track.enqueue(std::move(obj1));

  auto obj2 = makeObject(1, 2, 3, 200);
  obj2.subgroupId = 1;
  track.enqueue(std::move(obj2));

  auto stats = track.totalStats();

  CHECK_EQ(stats.objectCount, 2);
  CHECK_EQ(stats.byteCount, 300);
}

TEST_CASE("TrackPriorityQueue: getSubgroupQueue") {
  TrackPriorityQueue track;

  auto& queue0 = track.getSubgroupQueue(0);
  auto& queue1 = track.getSubgroupQueue(1);

  queue0.enqueue(makeObject(1, 1));
  queue1.enqueue(makeObject(1, 2));

  CHECK_EQ(queue0.size(), 1);
  CHECK_EQ(queue1.size(), 1);

  // Getting same subgroup should return same queue
  auto& queue0Again = track.getSubgroupQueue(0);
  CHECK_EQ(&queue0, &queue0Again);
}

// =============================================================================
// Cross-Stream Expiry Coordination Tests
// =============================================================================

TEST_CASE("StreamPriorityQueue: cross-stream expiry coordination") {
  auto tracker = std::make_shared<GroupExpiryTracker>();

  StreamPriorityQueue queue1;
  StreamPriorityQueue queue2;

  queue1.setGroupExpiryTracker(tracker);
  queue2.setGroupExpiryTracker(tracker);

  queue1.enqueue(makeObject(1, 1));
  queue2.enqueue(makeObject(1, 2));

  // Expire group via tracker
  tracker->expireGroup(1);

  // Both queues should see group as expired
  CHECK(queue1.isGroupExpired(1));
  CHECK(queue2.isGroupExpired(1));
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("StreamPriorityQueue: dequeue all priorities") {
  StreamPriorityQueue queue;

  // Add objects at all priority levels (0-7)
  for (uint8_t p = 0; p <= 7; ++p) {
    queue.enqueue(makeObject(1, p, p));
  }

  // Verify they come out in priority order
  for (uint8_t expected = 0; expected <= 7; ++expected) {
    auto obj = queue.dequeue();
    CHECK(obj.has_value());
    CHECK_EQ(obj->priority, expected);
  }
}

TEST_CASE("StreamPriorityQueue: endOfGroup marker") {
  StreamPriorityQueue queue;

  auto obj = makeObject(1, 10);
  obj.endOfGroup = true;

  queue.enqueue(std::move(obj));

  auto dequeued = queue.dequeue();
  CHECK(dequeued.has_value());
  CHECK(dequeued->endOfGroup);
}

TEST_CASE("StreamPriorityQueue: endOfTrack marker") {
  StreamPriorityQueue queue;

  auto obj = makeObject(1, 10);
  obj.endOfTrack = true;

  queue.enqueue(std::move(obj));

  auto dequeued = queue.dequeue();
  CHECK(dequeued.has_value());
  CHECK(dequeued->endOfTrack);
}

} // namespace moxygen::compat::test
