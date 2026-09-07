// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef JWPQT_CORE_QUERY_HISTORY_H
#define JWPQT_CORE_QUERY_HISTORY_H

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jwpqt::core {

class QueryHistoryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class QueryHistory {
 public:
  explicit QueryHistory(std::size_t storage_cells = 300);

  const std::vector<std::u32string>& entries() const noexcept;
  std::size_t storage_cells() const noexcept;
  std::size_t maximum_entries() const noexcept;
  std::size_t maximum_text_cells() const noexcept;
  std::optional<std::size_t> find(std::u32string_view text) const noexcept;
  bool remember(std::u32string_view text);
  void remove(std::size_t index);
  void set_storage_cells(std::size_t storage_cells);

 private:
  void prune();

  std::size_t storage_cells_ = 0;
  std::vector<std::u32string> entries_;
};

}  // namespace jwpqt::core

#endif
