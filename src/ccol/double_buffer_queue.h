//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//

#ifndef CONCURRENT_COLLECTIONS_DOUBLE_BUFFER_QUEUE_H_
#define CONCURRENT_COLLECTIONS_DOUBLE_BUFFER_QUEUE_H_

#include <ccol/common.h>

namespace ccol {

template <typename TElement>
class DoubleBufferQueue final {

 public:
  using BufferType = std::vector<TElement>;
  using TElementParam = std::conditional_t<std::is_trivially_copyable_v<TElement>, TElement, const TElement&>;

  using value_type = BufferType::value_type;
  using pointer = BufferType::pointer;
  using const_pointer = BufferType::const_pointer;
  using reference = BufferType::reference;
  using const_reference = BufferType::const_reference;
  using size_type = BufferType::size_type;
  using difference_type = BufferType::difference_type;
  using iterator = BufferType::iterator;
  using const_iterator = BufferType::const_iterator;
  using reverse_iterator = BufferType::reverse_iterator;
  using const_reverse_iterator = BufferType::const_reverse_iterator;

  void push_back(TElementParam element);
  bool is_back_buffer_empty() const;
  void swap_buffers();

  size_type size() const { return buffers_[active_buffer_.load()].size(); }

  void lock() const { active_buffer_mutex_.lock_shared(); }

  void unlock() const { active_buffer_mutex_.unlock_shared(); }

  [[nodiscard]] const_iterator begin() const { return buffers_[active_buffer_.load()].begin(); }

  [[nodiscard]] const_iterator end() const { return buffers_[active_buffer_.load()].end(); }

  [[nodiscard]] const_iterator rbegin() const { return buffers_[active_buffer_.load()].begin(); }

  [[nodiscard]] const_iterator rend() const { return buffers_[active_buffer_.load()].end(); }

  TElementParam operator[](size_type index) const { return buffers_[active_buffer_.load()][index]; }

  bool empty() const { return buffers_[active_buffer_.load()].empty(); }

 private:
  std::array<BufferType, 2> buffers_;
  mutable std::mutex back_buffer_mutex_;
  mutable std::shared_mutex active_buffer_mutex_;
  std::atomic<std::uint8_t> active_buffer_;
};

template <typename TElement>
bool DoubleBufferQueue<TElement>::is_back_buffer_empty() const {
  std::lock_guard _scoped_lock(back_buffer_mutex_);
  return buffers_[active_buffer_.load() ^ 1].empty();
}

template <typename TElement>
void DoubleBufferQueue<TElement>::swap_buffers() {
  std::scoped_lock _scoped_lock(back_buffer_mutex_, active_buffer_mutex_);
  const std::uint8_t last_active_buffer = active_buffer_.fetch_xor(1);
  buffers_[last_active_buffer].clear();
}

template <typename TElement>
void DoubleBufferQueue<TElement>::push_back(TElementParam element) {
  std::lock_guard _scoped_lock(back_buffer_mutex_);
  buffers_[active_buffer_.load() ^ 1].push_back(element);
}

}  // namespace ccol

#endif  // CONCURRENT_COLLECTIONS_DOUBLE_BUFFER_QUEUE_H_
