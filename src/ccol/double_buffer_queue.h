//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//

#ifndef CONCURRENT_COLLECTIONS_DOUBLE_BUFFER_QUEUE_H_
#define CONCURRENT_COLLECTIONS_DOUBLE_BUFFER_QUEUE_H_

#include <ccol/common.h>
#include <ccol/spinlock.h>
#include <ccol/trivial_vector.h>
#include <ccol/sparse_vector.h>

namespace ccol {

/// \brief A double buffer queue is a queue class that allows multiple writers to add to a back buffer
/// and multiple readers to access a front read-only buffer
template <typename T>
class double_buffer_queue final {

 public:
  using TCollection = std::conditional_t<std::is_trivially_copyable_v<T>, trivial_vector<T>, sparse_vector<T, 64>>;
  using TParam = std::conditional_t<std::is_trivially_copyable_v<T>, T, const T&>;

  using value_type = TCollection::value_type;
  using size_type = TCollection::size_type;
  using difference_type = TCollection::difference_type;
  using iterator = TCollection::iterator;
  using const_iterator = TCollection::const_iterator;
  using reverse_iterator = TCollection::reverse_iterator;
  using const_reverse_iterator = TCollection::const_reverse_iterator;

  double_buffer_queue() = default;
  double_buffer_queue(const double_buffer_queue&) = delete;
  double_buffer_queue(double_buffer_queue&&) = delete;
  double_buffer_queue& operator=(const double_buffer_queue&) = delete;
  double_buffer_queue& operator=(double_buffer_queue&&) = delete;
  ~double_buffer_queue() = default;

  /// \brief Safely push back a value to the back buffer
  void push_back(TParam element) {
    std::lock_guard _scoped_lock(back_buffer_mutex_);
    buffers_[front_buffer_.load() ^ 1].push_back(element);
  }

  /// \brief Check if back buffer has values before swapping
  bool is_back_buffer_empty() const{
    std::lock_guard _scoped_lock(back_buffer_mutex_);
    return buffers_[front_buffer_.load() ^ 1].empty();
  }

  /// \brief Safely swap buffers (will wait for readers and writers)
  void swap_buffers() {
    std::scoped_lock _scoped_lock(back_buffer_mutex_, front_buffer_mutex_);
    const std::uint8_t last_active_buffer = front_buffer_.fetch_xor(1);
    buffers_[last_active_buffer].clear();
  }

  /// \brief Get the front buffer size
  size_type size() const { return buffers_[front_buffer_.load()].size(); }
  /// \brief Marks the front buffer as being read. You need to call unlock() when done.
  /// \see DoubleBufferQueue::unlock()
  void lock() const { front_buffer_mutex_.lock_shared(); }
  /// \brief Unlocks the front buffer from reading.
  void unlock() const { front_buffer_mutex_.unlock_shared(); }
  [[nodiscard]] const_iterator begin() const { return buffers_[front_buffer_.load()].begin(); }
  [[nodiscard]] const_iterator end() const { return buffers_[front_buffer_.load()].end(); }
  [[nodiscard]] const_iterator rbegin() const { return buffers_[front_buffer_.load()].begin(); }
  [[nodiscard]] const_iterator rend() const { return buffers_[front_buffer_.load()].end(); }
  TParam operator[](size_type index) const { return buffers_[front_buffer_.load()][index]; }
  /// \brief Checks if the front buffer has any values.
  bool empty() const { return buffers_[front_buffer_.load()].empty(); }

 private:
  std::array<TCollection, 2> buffers_;
  mutable spin_mutex back_buffer_mutex_;
  mutable shared_spin_mutex front_buffer_mutex_;
  std::atomic<std::uint8_t> front_buffer_;
};

}  // namespace ccol

#endif  // CONCURRENT_COLLECTIONS_DOUBLE_BUFFER_QUEUE_H_
