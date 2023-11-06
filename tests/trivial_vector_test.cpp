//
// Copyright (c) 2023 Paper Cranes Ltd.
// All rights reserved.
//
#include <catch2/catch_all.hpp>
#include <ccol/trivial_vector.h>

#include <barrier>

struct TrivialPoint final {
  TrivialPoint() = default;

  TrivialPoint(std::float_t x_, std::float_t y_)
      : x(x_), y(y_) {}

  TrivialPoint(const TrivialPoint&) = default;
  TrivialPoint& operator=(const TrivialPoint&) = default;

  [[nodiscard]] bool equals(const TrivialPoint& other) const { return x == other.x && y == other.y; }

  [[nodiscard]] bool operator==(const TrivialPoint& other) const { return equals(other); }

  [[nodiscard]] bool operator!=(const TrivialPoint& other) const { return !equals(other); }

  std::float_t x = 0.0f;
  std::float_t y = 0.0f;
};

TEST_CASE("TrivialVector Basic Operations", "[tvector]") {
  SECTION("Integer Elements") {
    ccol::trivial_vector<std::uint32_t> elements;
    elements.push_back(0);
    elements.push_back(1);
    elements.push_back(2);

    CHECK(elements.size() == 3);  // NOLINT(*-container-size-empty)

    for (std::uint32_t i = 0; i < elements.size(); i++) {
      CHECK(elements[i] == i);
    }

    std::uint32_t ix = 0;
    for (std::uint32_t element : elements) {
      CHECK(element == ix++);
    }
  }

  SECTION("Pointer Elements") {
    std::array<std::uint32_t, 2> row{0, 1};
    ccol::trivial_vector<std::uint32_t*> elements;
    elements.push_back(row.data());

    CHECK(elements.size() == 1);  // NOLINT(*-container-size-empty)

    CHECK(elements[0] == row.data());
    CHECK(elements[0][0] == 0);
    CHECK(elements[0][1] == 1);
  }

  SECTION("Trivial Struct Elements") {
    ccol::trivial_vector<TrivialPoint> elements;
    elements.push_back({0.0f, 0.0f});
    elements.push_back({0.0f, 1.0f});
    elements.push_back({1.0f, 0.0f});
    elements.push_back({1.0f, 1.0f});

    CHECK(elements.size() == 4);  // NOLINT(*-container-size-empty)

    CHECK(elements[0] == TrivialPoint{0.0f, 0.0f});
  }
}

TEST_CASE("TrivialVector MT Access", "[tvector][!mayfail][!throws]") {
  ccol::trivial_vector<std::uint32_t> elements;
  std::barrier sync_point(2);

  {
    std::jthread push_thread([&elements, &sync_point]() {
      sync_point.arrive_and_wait();
      for (std::uint32_t i = 0; i < 256000; i++) {
        elements.push_back(i);
        std::this_thread::yield();
      }
    });

    sync_point.arrive_and_wait();
    std::this_thread::yield();

    SECTION("Access Validation") {
      CHECK(elements.size() > 0);
      for (std::uint32_t i = 0; i < elements.size(); i++) {
        CHECK(elements[i] == i);
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
