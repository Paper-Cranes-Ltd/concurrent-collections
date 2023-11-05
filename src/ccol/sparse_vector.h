//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//

#ifndef CONCURRENTCOLLECTIONS_SPARSE_VECTOR_H_
#define CONCURRENTCOLLECTIONS_SPARSE_VECTOR_H_

#include <ccol/common.h>
#include <ccol/trivial_vector.h>
#include <EASTL/fixed_vector.h>
#include <ccol/spinlock.h>

namespace ccol {

/// \brief A collection that allows reference access to complex elements more safely.
template <typename TElement, std::size_t TBucketSize>
class SparseVector final {
 public:
  using value_type = TElement;
  using size_type = std::size_t;
  using difference_type = std::size_t;

  SparseVector() = default;
  SparseVector(SparseVector&&) = default;
  SparseVector& operator=(SparseVector&&) = default;
  SparseVector(const SparseVector&) = delete;
  SparseVector& operator=(const SparseVector&) = delete;
  ~SparseVector() { free(); }

  void push_back(const value_type& new_element);
  void emplace_back(value_type&& new_element);

  [[nodiscard]] size_type size() const { return size_.load(); }

  /// \brief Sets the vector size to 0 but doesn't free any memory.
  void clear() { size_.store(0); }
  /// \brief Frees the arrays.
  void free();

  const value_type& operator[](size_type index) const { return pages_[index / TBucketSize]->at(index % TBucketSize); }

 private:
  void create_page_for(size_type index);

  using page_type = eastl::fixed_vector<TElement, TBucketSize, /* allocate on overflow */ false>;

  TrivialVector<page_type*> pages_;
  std::atomic<std::size_t> size_ = 0;
  mutable SpinLock page_lock_;
  mutable SpinLock write_lock_;
};

template <typename TElement, std::size_t TBucketSize>
void SparseVector<TElement, TBucketSize>::free() {
  std::scoped_lock scoped_lock(page_lock_, write_lock_);

  clear();
  for(page_type* page : pages_) {
    delete page;
  }

  pages_.clear();
}

template <typename TElement, std::size_t TBucketSize>
void SparseVector<TElement, TBucketSize>::create_page_for(size_type index) {
  std::lock_guard _scoped_lock(page_lock_);
  if (index / TBucketSize < pages_.size()) {
    return;
  }

  pages_.push_back(new page_type());
}

template <typename TElement, std::size_t TBucketSize>
void SparseVector<TElement, TBucketSize>::push_back(const value_type& new_element) {
  std::lock_guard _scoped_lock(write_lock_);

  const size_type placement_position = size_++;
  const size_type page_index = placement_position / TBucketSize;

  if (page_index >= pages_.size()) {
    create_page_for(placement_position);
  }

  page_type* page = pages_[page_index];
  page->push_back(new_element);
}

template <typename TElement, std::size_t TBucketSize>
void SparseVector<TElement, TBucketSize>::emplace_back(value_type&& new_element) {
  std::lock_guard _scoped_lock(write_lock_);

  const size_type placement_position = size_++;
  const size_type page_index = placement_position / TBucketSize;

  if (page_index >= pages_.size()) {
    create_page_for(placement_position);
  }

  page_type* page = pages_[page_index];
  page->emplace_back(std::move(new_element));
}

}  // namespace ccol

#endif  // CONCURRENTCOLLECTIONS_SPARSE_VECTOR_H_
