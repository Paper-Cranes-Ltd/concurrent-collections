//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//

#ifndef CONCURRENT_COLLECTIONS_COMMON_H_
#define CONCURRENT_COLLECTIONS_COMMON_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace ccol {

class SpinLock {
 public:
  void lock() {
    while(!try_lock()) {
      while (lock_.load(std::memory_order_relaxed)) {
#if defined(__GNUC__) || defined(_MSC_VER)
        _mm_pause();
#else
        (void)0;
#endif
      }
    }
  }

  bool try_lock() {
    return !lock_.exchange(true, std::memory_order_acquire);
  }

  void unlock() {
    lock_.store(false, std::memory_order_release);
  }

 private:
  std::atomic<bool> lock_ = {false};
};

}

#endif  // CONCURRENT_COLLECTIONS_COMMON_H_
