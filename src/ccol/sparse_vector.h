//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//

#ifndef CONCURRENTCOLLECTIONS_SPARSE_VECTOR_H_
#define CONCURRENTCOLLECTIONS_SPARSE_VECTOR_H_

#include <ccol/common.h>
#include <ccol/spinlock.h>
#include <ccol/trivial_vector.h>

namespace ccol {

/// \brief A collection that allows reference access to complex elements more safely.
template <typename T, std::size_t TBucketSize>
class sparse_vector final {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::size_t;

  struct iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = sparse_vector::value_type;
    using pointer = value_type*;
    using reference = value_type&;

    explicit iterator(sparse_vector& tv_, sparse_vector::size_type index_)
        : index(index_), tv(tv_) {}

    reference operator*() const { return tv[index]; }
    pointer operator->() { return &tv[index]; }

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

    sparse_vector::size_type index = 0;
    const sparse_vector& tv;
  };

  struct const_iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = const sparse_vector::value_type;
    using pointer = const value_type*;
    using reference = const value_type&;

    explicit const_iterator(const sparse_vector& tv_, sparse_vector::size_type index_)
        : index(index_), tv(tv_) {}

    reference operator*() const { return tv[index]; }
    pointer operator->() { return &tv[index]; }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      index++;
      return tmp;
    }

    const_iterator& operator++() {
      ++index;
      return *this;
    }

    const_iterator& operator+=(size_type amount) {
      index += amount;
      return *this;
    }

    const_iterator operator--(int) {
      const_iterator tmp = *this;
      index--;
      return tmp;
    }

    const_iterator& operator--() {
      --index;
      return *this;
    }

    const_iterator& operator-=(size_type amount) {
      index -= amount;
      return *this;
    }

    bool equals(const const_iterator& other) const { return &tv == &other.tv && index == other.index; }
    bool operator==(const const_iterator& other) const { return equals(other); };
    bool operator!=(const const_iterator& other) const { return !equals(other); };

    sparse_vector::size_type index = 0;
    sparse_vector& tv;
  };

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  sparse_vector() = default;
  sparse_vector(sparse_vector&&) = default;
  sparse_vector& operator=(sparse_vector&&) = default;
  sparse_vector(const sparse_vector&) = delete;
  sparse_vector& operator=(const sparse_vector&) = delete;
  ~sparse_vector() { free(); }

  void emplace_back(value_type&& new_element) {
    std::lock_guard _scoped_lock(write_lock_);

    const size_type placement_position = size_++;
    const size_type page_index = placement_position / TBucketSize;

    if (page_index >= pages_.size()) {
      create_page_for(placement_position);
    }

    page_type* page = pages_[page_index];
    page->emplace_back(std::move(new_element));
  }

  /// \brief Frees the arrays.
  /// \warning Calling this function is not thread safe.
  /// It will delete the contents of the collection and
  /// might result in invalidating iterators and references
  /// that were returned.
  void free() {
    std::scoped_lock scoped_lock(page_lock_, write_lock_);

    clear();
    for (page_type* page : pages_) {
      delete page;
    }

    pages_.clear();
  }

  void push_back(const value_type& new_element) {
    std::lock_guard _scoped_lock(write_lock_);

    const size_type placement_position = size_++;
    const size_type page_index = placement_position / TBucketSize;

    if (page_index >= pages_.size()) {
      create_page_for(placement_position);
    }

    page_type* page = pages_[page_index];
    page->push_back(new_element);
  }

  [[nodiscard]] size_type size() const { return size_.load(); }
  /// \brief Sets the vector size to 0 but doesn't free any memory.
  void clear() { size_.store(0); }
  const value_type& operator[](size_type index) const { return pages_[index / TBucketSize]->at(index % TBucketSize); }
  value_type operator[](size_type index) { return pages_[index / TBucketSize]->at(index % TBucketSize); }

  const_iterator begin() const { return const_iterator(*this, 0); }
  const_iterator end() const { return const_iterator(*this, size()); }
  iterator begin() { return iterator(*this, 0); }
  iterator end() { return iterator(*this, size()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(*this, 0); }
  const_reverse_iterator rend() const { return const_reverse_iterator(*this, size()); }
  reverse_iterator rbegin() { return reverse_iterator(*this, 0); }
  reverse_iterator rend() { return reverse_iterator(*this, size()); }

 private:
  using page_type = std::vector<T>;

  void create_page_for(size_type index) {
    std::lock_guard _scoped_lock(page_lock_);
    if (index / TBucketSize < pages_.size()) {
      return;
    }

    auto* page = new page_type();
    page->reserve(TBucketSize);
    pages_.push_back(page);
  }

  trivial_vector<page_type*> pages_;
  std::atomic<std::size_t> size_ = 0;
  mutable spin_mutex page_lock_;
  mutable spin_mutex write_lock_;
};

}  // namespace ccol

#endif  // CONCURRENTCOLLECTIONS_SPARSE_VECTOR_H_
