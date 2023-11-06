//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//
#include <catch2/catch_all.hpp>
#include <ccol/sparse_vector.h>

#include <barrier>

struct ConstructorDestructorTester {
  static std::atomic<std::int32_t> construction_count;
  static std::atomic<std::int32_t> copy_count;
  static std::atomic<std::int32_t> move_count;
  static std::atomic<std::int32_t> destruction_count;

  ConstructorDestructorTester() { construction_count++; }

  ConstructorDestructorTester(const ConstructorDestructorTester&) { copy_count++; }

  ConstructorDestructorTester& operator=(const ConstructorDestructorTester&) { copy_count++; }

  ConstructorDestructorTester(ConstructorDestructorTester&&) noexcept { move_count++; }

  ConstructorDestructorTester& operator=(ConstructorDestructorTester&&) noexcept { move_count++; }

  ~ConstructorDestructorTester() { destruction_count++; }
};

std::atomic<std::int32_t> ConstructorDestructorTester::construction_count = 0;
std::atomic<std::int32_t> ConstructorDestructorTester::copy_count = 0;
std::atomic<std::int32_t> ConstructorDestructorTester::move_count = 0;
std::atomic<std::int32_t> ConstructorDestructorTester::destruction_count = 0;

TEST_CASE("SparseVector Basic Operations", "[svector]") {
  SECTION("Basic String") {
    ccol::sparse_vector<std::string, 1024> elements;
    elements.emplace_back("0");
    elements.push_back("1");
    elements.push_back("2");

    CHECK(elements.size() == 3);  // NOLINT(*-container-size-empty)
    CHECK(elements[0] == "0");
    CHECK(elements[1] == "1");
    CHECK(elements[2] == "2");
  }

  SECTION("Basic String Page Overflow") {
    ccol::sparse_vector<std::string, 2> elements;
    elements.emplace_back("0");
    elements.push_back("1");
    elements.push_back("2");

    CHECK(elements.size() == 3);  // NOLINT(*-container-size-empty)
    CHECK(elements[0] == "0");
    CHECK(elements[1] == "1");
    CHECK(elements[2] == "2");
  }

  SECTION("Construction/Destruction Test") {
    ccol::sparse_vector<ConstructorDestructorTester, 512> elements;
    elements.emplace_back({});
    elements.push_back({});

    CHECK(ConstructorDestructorTester::construction_count == 2);
    CHECK(ConstructorDestructorTester::move_count == 1);
    CHECK(ConstructorDestructorTester::copy_count == 1);
    CHECK(ConstructorDestructorTester::destruction_count == 2);

    elements.free();

    CHECK(ConstructorDestructorTester::destruction_count == 4);
  }
}

TEST_CASE("SparseVector MT Access", "[svector][!mayfail][!throws]") {
  ccol::sparse_vector<std::uint32_t, 1024> elements;
  std::barrier sync_point(3);

  {
    std::jthread push_thread_1([&elements, &sync_point]() {
      sync_point.arrive_and_wait();
      for (std::uint32_t i = 1; i < 128000; i++) {
        elements.push_back(i);
        std::this_thread::yield();
      }
    });

    std::jthread push_thread_2([&elements, &sync_point]() {
      sync_point.arrive_and_wait();
      for (std::uint32_t i = 1; i < 128000; i++) {
        elements.push_back(i);
        std::this_thread::yield();
      }
    });

    sync_point.arrive_and_wait();
    std::this_thread::yield();

    SECTION("Access Validation") {
      CHECK(elements.size() > 0);
      for (std::uint32_t i = 0; i < elements.size(); i++) {
        CHECK(elements[i] > 0);
      }
    }

    SECTION("Benchmark MT", "") {
      std::int32_t size = std::min<std::int32_t>(100, static_cast<std::int32_t>(elements.size()));
      BENCHMARK("Single Random Element") {
        return elements[size - 1];
      };

      BENCHMARK("Sum 0-100") {
        std::uint32_t sum = 0;
        for (std::uint32_t i = 0; i < size; i++) {
          sum += elements[i];
        }
        return sum;
      };
    }
  }

  SECTION("Benchmark ST") {
    BENCHMARK("Sum 0-100") {
      std::uint32_t sum = 0;
      for (std::uint32_t i = 0; i < 100; i++) {
        sum += elements[i];
      }
      return sum;
    };
  }
}
