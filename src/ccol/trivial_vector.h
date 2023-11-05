//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//

#ifndef CONCURRENTCOLLECTIONS_CONCURRENT_POINTER_VECTOR_H_
#define CONCURRENTCOLLECTIONS_CONCURRENT_POINTER_VECTOR_H_

#include <ccol/common.h>
#include <ccol/spinlock.h>

namespace ccol {

/// \brief A resizable collection for trivial types.
/// \details Since trivial types are easy to copy we can make an easy to use collection that can
/// be read to while being resized or written to safely. Any modification still includes
/// a locking mechanism which is why read access copies the elements.
template <typename TElement>
class TrivialVector final {
 public:
  static_assert(std::is_trivially_copyable_v<TElement>, "Trivial vector can only contain trivial elements");
  using value_type = TElement;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  struct iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = TrivialVector::value_type;
    using pointer = value_type;
    using reference = value_type;

    explicit iterator(const TrivialVector& tv_, TrivialVector::size_type index_)
        : index(index_), tv(tv_){};

    value_type operator*() const { return tv[index]; }

    value_type operator->() { return tv[index]; }

    iterator operator++(int) {
      iterator tmp = *this;
      index++;
      return tmp;
    }

    iterator& operator++() {
      ++index;
      return *this;
    }

    iterator& operator+=(size_type amount) {
      index += amount;
      return *this;
    }

    iterator operator--(int) {
      iterator tmp = *this;
      index--;
      return tmp;
    }

    iterator& operator--() {
      --index;
      return *this;
    }

    iterator& operator-=(size_type amount) {
      index -= amount;
      return *this;
    }

    bool equals(const iterator& other) const { return &tv == &other.tv && index == other.index; }

    bool operator==(const iterator& other) const { return equals(other); };

    bool operator!=(const iterator& other) const { return !equals(other); };

    TrivialVector::size_type index = 0;
    const TrivialVector& tv;
  };

  value_type operator[](size_type index) const;

  size_type size() const { return size_.load(); }

  void clear() { resize(0); }

  void resize(size_type new_size);
  void push_back(value_type new_value);
  void replace(size_type index, value_type new_value);
  [[nodiscard]] value_type exchange(size_type index, value_type new_value);

  iterator begin() const { return iterator(*this, 0); }
  iterator end() const { return iterator(*this, size()); }

 private:
  std::array<std::atomic<value_type*>, 2> safe_buffers_{nullptr, nullptr};
  mutable RWSpinLock reader_mutex_;
  mutable SpinLock write_mutex_;
  std::atomic<std::uint8_t> active_buffer_ = 0;
  std::atomic<size_type> size_ = 0;
  std::atomic<size_type> reserved_ = 0;

  void resize_no_lock(size_type new_size);
};

template <typename TElement>
TrivialVector<TElement>::value_type TrivialVector<TElement>::exchange(size_type index, value_type new_value) {
  std::lock_guard _scoped_lock(write_mutex_);
  value_type old = safe_buffers_[active_buffer_.load()][index];
  safe_buffers_[active_buffer_.load()][index] = new_value;
  return old;
}

template <typename TElement>
void TrivialVector<TElement>::replace(TrivialVector::size_type index, value_type new_value) {
  std::lock_guard _scoped_lock(write_mutex_);
  safe_buffers_[active_buffer_.load()][index] = new_value;
}

template <typename TElement>
void TrivialVector<TElement>::push_back(value_type new_value) {
  std::lock_guard _scoped_lock(write_mutex_);
  resize_no_lock(size() + 1);
  safe_buffers_[active_buffer_.load()][size() - 1] = new_value;
}

template <typename TElement>
TrivialVector<TElement>::value_type TrivialVector<TElement>::operator[](TrivialVector::size_type index) const {
  std::shared_lock _scoped_lock(reader_mutex_);
  return safe_buffers_[active_buffer_.load()][index];
}

template <typename TElement>
void TrivialVector<TElement>::resize(TrivialVector::size_type new_size) {
  std::lock_guard _scoped_lock(write_mutex_);
  resize_no_lock(new_size);
}

template <typename TElement>
void TrivialVector<TElement>::resize_no_lock(size_type new_size) {
  if (size() >= new_size) {
    size_.store(new_size);
    return;
  }

  if (reserved_.load() < new_size) {
    size_type new_reserved = reserved_.load() == 0 ? 2 : reserved_.load() * 2;
    auto* new_buffer = new value_type[new_reserved];
    if (size() > 0) {
      auto* current_active_buffer = safe_buffers_[active_buffer_.load()].load();
      std::copy(current_active_buffer, current_active_buffer + size(), new_buffer);
    }
    std::fill(new_buffer + size(), new_buffer + new_size, TElement{});

    // sanity check make sure back buffer is free
    auto* old_buffer = safe_buffers_[active_buffer_.load() ^ 1].exchange(new_buffer);
    delete[] old_buffer;

    reader_mutex_.lock();
    std::uint8_t back_index_after_swap = active_buffer_.fetch_xor(1);
    size_.store(new_size);
    reserved_.store(new_reserved);
    reader_mutex_.unlock();

    // free back buffer after there are no readers
    old_buffer = safe_buffers_[back_index_after_swap].exchange(nullptr);
    delete[] old_buffer;
  } else {
    size_.store(new_size);
  }
}

}  // namespace ccol

#endif  // CONCURRENTCOLLECTIONS_CONCURRENT_POINTER_VECTOR_H_
