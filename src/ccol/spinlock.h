//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//

#ifndef CONCURRENTCOLLECTIONS_SPINLOCK_H_
#define CONCURRENTCOLLECTIONS_SPINLOCK_H_

#include <ccol/common.h>

namespace ccol {

class SpinLock {
 public:
  void lock() {
    while(!try_lock()) {
      while (lock_.load(std::memory_order_relaxed)) {
        noop();
      }
    }
  }

  bool try_lock() {
    return !lock_.exchange(true, std::memory_order_acquire);
  }

  bool is_locked() const {
    return lock_.load(std::memory_order_relaxed);
  }

  void unlock() {
    lock_.store(false, std::memory_order_release);
  }

 protected:
  static void noop() {
#if defined(__GNUC__) || defined(_MSC_VER)
    _mm_pause();
#else
    (void)0;
#endif
  }

 private:
  std::atomic<bool> lock_ = {false};
};

class RWSpinLock : protected SpinLock {
 public:
  using SpinLock::try_lock;
  using SpinLock::is_locked;

  void lock() {
    SpinLock::lock();
    while(read_count_.load(std::memory_order_relaxed) > 0) {
      noop();
    }
  }

  void unlock() {
    SpinLock::unlock();
  }

  void lock_shared() {
    SpinLock::lock();
    read_count_++;
    SpinLock::unlock();
  }

  void unlock_shared() {
    read_count_--;
  }

 private:
  std::atomic<std::int32_t> read_count_ = {0};
};

}

#endif  // CONCURRENTCOLLECTIONS_SPINLOCK_H_
