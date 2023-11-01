//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//
#include <catch2/catch_all.hpp>
#include <ccol/double_buffer_queue.h>

#include <barrier>

TEST_CASE("DoubleBufferQueue Basic Operations", "[dbqueue]") {
  ccol::DoubleBufferQueue<std::uint32_t> queue;
  queue.push_back(0);
  queue.push_back(1);
  queue.push_back(2);

  CHECK(queue.size() == 0); // NOLINT(*-container-size-empty)

  queue.swap_buffers();

  queue.lock();
  CHECK(queue.size() == 3);
  CHECK(queue[0] == 0);
  CHECK(queue[1] == 1);
  CHECK(queue[2] == 2);
  queue.unlock();
}

TEST_CASE("DoubleBufferQueue MT Access", "[dbqueue][!mayfail][!throws]") {
  ccol::DoubleBufferQueue<std::uint32_t> queue;
  std::barrier sync_point(2);

  std::jthread push_thread([&queue, &sync_point]() {
    sync_point.arrive_and_wait();
    for (std::uint32_t i = 0; i < 256000; i++) {
      queue.push_back(i);
      std::this_thread::yield();
    }
  });

  sync_point.arrive_and_wait();
  std::this_thread::yield();
  queue.swap_buffers();
  queue.lock();
  CHECK(!queue.empty());
  CHECK(queue.size() < 256000);

  if(!queue.empty()) {
    CHECK(queue[0] == 0);
  }

  std::uint32_t last_size = queue.size();

  CHECK(!queue.is_back_buffer_empty());
  queue.unlock();

  std::this_thread::yield();
  queue.swap_buffers();
  queue.lock();
  CHECK(!queue.empty());
  CHECK(queue.size() < 256000);
  if(!queue.empty()) {
    CHECK(queue[0] == last_size);
  }
  queue.unlock();
}
