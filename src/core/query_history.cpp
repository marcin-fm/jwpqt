// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/query_history.h"

namespace jwpqt::core {

QueryHistory::QueryHistory(std::size_t storage_cells) {
  set_storage_cells(storage_cells);
}

const std::vector<std::u32string>& QueryHistory::entries() const noexcept {
  return entries_;
}

std::size_t QueryHistory::storage_cells() const noexcept { return storage_cells_; }

std::size_t QueryHistory::maximum_entries() const noexcept {
  return maximum_text_cells() == 0 ? 0 : storage_cells_ / 10 + 1;
}

std::size_t QueryHistory::maximum_text_cells() const noexcept {
  // Reserve the legacy pointer table, sentinel and strict end-of-buffer bound.
  const std::size_t overhead = storage_cells_ / 10 + 3;
  return storage_cells_ > overhead ? storage_cells_ - overhead : 0;
}

std::optional<std::size_t> QueryHistory::find(std::u32string_view text) const noexcept {
  if (text.empty() || text.size() > maximum_text_cells()) return std::nullopt;
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i] == text) return i;
  }
  return std::nullopt;
}

bool QueryHistory::remember(std::u32string_view text) {
  if (text.empty() || text.size() > maximum_text_cells()) return false;
  for (char32_t character : text) {
    if ((character < 0x20 && character != U'\t') ||
        (character >= 0x7f && character <= 0x9f) ||
        character == U'\u2028' || character == U'\u2029' || character > 0x10ffff ||
        (character >= 0xd800 && character <= 0xdfff)) {
      throw QueryHistoryError("History entries must contain valid single-line Unicode");
    }
  }
  const auto previous = find(text);
  if (previous && *previous == 0) return false;
  QueryHistory candidate = *this;
  if (previous) candidate.remove(*previous);
  candidate.entries_.insert(candidate.entries_.begin(), std::u32string(text));
  candidate.prune();
  entries_.swap(candidate.entries_);
  return true;
}

void QueryHistory::remove(std::size_t index) {
  if (index >= entries_.size()) throw QueryHistoryError("History index is out of range");
  entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
}

void QueryHistory::set_storage_cells(std::size_t storage_cells) {
  if (storage_cells > 30000) throw QueryHistoryError("History storage exceeds 30000 cells");
  if (storage_cells == storage_cells_) return;
  QueryHistory candidate = *this;
  candidate.storage_cells_ = storage_cells;
  candidate.prune();
  entries_.swap(candidate.entries_);
  storage_cells_ = storage_cells;
}

void QueryHistory::prune() {
  std::size_t characters = 0;
  for (const auto& entry : entries_) characters += entry.size();
  while (entries_.size() > maximum_entries() || characters > maximum_text_cells()) {
    characters -= entries_.back().size();
    entries_.pop_back();
  }
}

}  // namespace jwpqt::core
