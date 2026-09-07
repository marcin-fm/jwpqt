// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/query_history.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void rejected(Function function) {
  try {
    function();
  } catch (const jwpqt::core::QueryHistoryError&) {
    return;
  }
  throw std::runtime_error("Invalid query history operation was accepted");
}

void test_history() {
  using jwpqt::core::QueryHistory;
  QueryHistory history;
  require(history.storage_cells() == 300 && history.maximum_entries() == 31 &&
              history.maximum_text_cells() == 267 && history.entries().empty(),
          "Default history capacity differs from the source buffer layout");
  require(!history.remember(U"") && history.remember(U"first") &&
              history.remember(U"second") && history.remember(U"first") &&
              !history.remember(U"first") && history.remember(U"First") &&
              history.entries() == std::vector<std::u32string>{U"First", U"first", U"second"} &&
              history.find(U"first") == 1 && !history.find(U"missing"),
          "History did not promote exact duplicates without case folding");
  history.remove(1);
  require(history.entries() == std::vector<std::u32string>{U"First", U"second"},
          "Removing a history entry changed the remaining order");
  const auto before = history.entries();
  rejected([&] { history.remove(2); });
  rejected([&] { history.set_storage_cells(30001); });
  rejected([&] { history.set_storage_cells(std::numeric_limits<std::size_t>::max()); });
  for (const std::u32string& text : {std::u32string{0}, std::u32string{0xd800},
                                   std::u32string{0xdfff}, std::u32string{0x110000},
                                   std::u32string{0x1f}, std::u32string{0x7f}, std::u32string{0x85},
                                   std::u32string{U'a', U'\n'}, std::u32string{U'\r'},
                                   std::u32string{U'\u2028'}, std::u32string{U'\u2029'}}) {
    rejected([&] { history.remember(text); });
  }
  require(history.entries() == before && history.storage_cells() == 300,
          "Rejected history changes mutated the previous state");
  require(history.remember(U"\ufeff\u00a0\U0001f600\t") &&
              history.entries().front() == U"\ufeff\u00a0\U0001f600\t",
          "History normalized signature, spacing, supplementary or tab characters");

  QueryHistory count_bound;
  for (char32_t i = 0; i < 40; ++i) {
    require(count_bound.remember(std::u32string(1, 0x4e00 + i)),
            "A distinct bounded query was not remembered");
  }
  require(count_bound.entries().size() == 31 &&
              count_bound.entries().front() == std::u32string(1, 0x4e00 + 39) &&
              count_bound.entries().back() == std::u32string(1, 0x4e00 + 9),
          "History count eviction did not remove the oldest entries");
  const auto bounded = count_bound.entries();
  require(!count_bound.remember(std::u32string(268, U'x')) &&
              count_bound.entries() == bounded,
          "An overlong query was truncated or evicted existing history");
  require(count_bound.remember(std::u32string(267, U'x')) &&
              count_bound.entries().size() == 1 && count_bound.entries().front().size() == 267,
          "Exact history text capacity was not accepted");
  require(count_bound.remember(U"y") && count_bound.entries() == std::vector<std::u32string>{U"y"},
          "History text eviction did not discard the oldest oversized remainder");
  count_bound.set_storage_cells(10);
  require(count_bound.maximum_entries() == 2 && count_bound.maximum_text_cells() == 6,
          "Resized history capacity is wrong");
  count_bound.remember(U"ab");
  count_bound.remember(U"cd");
  count_bound.set_storage_cells(6);
  require(count_bound.entries() == std::vector<std::u32string>{U"cd"},
          "Shrinking history did not preserve its newest fitting entry");
  for (std::size_t capacity = 0; capacity <= 3; ++capacity) {
    count_bound.set_storage_cells(capacity);
    require(count_bound.entries().empty() && count_bound.maximum_entries() == 0 &&
                count_bound.maximum_text_cells() == 0 && !count_bound.remember(U"a"),
            "An empty or undersized history buffer underflowed its capacity");
  }
  count_bound.set_storage_cells(30000);
  require(count_bound.maximum_entries() == 3001 && count_bound.maximum_text_cells() == 26997 &&
              count_bound.remember(std::u32string(26997, U'x')) &&
              !count_bound.remember(std::u32string(26998, U'x')),
          "The maximum supported history buffer lost its exact bounds");
}

}  // namespace

int main() {
  try {
    test_history();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
