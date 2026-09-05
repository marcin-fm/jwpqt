// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_session.h"

#include <utility>

namespace jwpqt::core {

WnnConversionSession::WnnConversionSession(
    const WnnDictionary& system_dictionary, WnnPreferences& preferences,
    const std::vector<WnnRecord>* user_records,
    std::size_t maximum_output_cells)
    : system_dictionary_(system_dictionary),
      preferences_(preferences),
      user_records_(user_records),
      maximum_output_cells_(maximum_output_cells) {}

bool WnnConversionSession::begin(const JwpText& input) {
  JwpText next_input = input;
  WnnLookupResult next_result = lookup_wnn_candidates(
      system_dictionary_, input, user_records_, maximum_output_cells_);
  if (next_result.candidates.empty()) {
    clear();
    return false;
  }
  const std::optional<std::size_t> preferred =
      preferences_.preferred_candidate(input, next_result);
  if (!preferred || *preferred >= next_result.candidates.size()) {
    throw WnnSessionError("WNN preferences returned an invalid candidate");
  }

  preferences_.remember(input, next_result, *preferred);
  input_.swap(next_input);
  result_.candidates.swap(next_result.candidates);
  std::swap(result_.can_extend, next_result.can_extend);
  selected_index_ = *preferred;
  active_ = true;
  return true;
}

void WnnConversionSession::clear() noexcept {
  JwpText{}.swap(input_);
  std::vector<WnnCandidate>{}.swap(result_.candidates);
  result_.can_extend = false;
  selected_index_ = 0;
  active_ = false;
}

bool WnnConversionSession::active() const noexcept { return active_; }

const JwpText& WnnConversionSession::input() const {
  require_active();
  return input_;
}

const WnnLookupResult& WnnConversionSession::result() const {
  require_active();
  return result_;
}

std::size_t WnnConversionSession::selected_index() const {
  require_active();
  return selected_index_;
}

const WnnCandidate& WnnConversionSession::selected_candidate() const {
  require_active();
  return result_.candidates[selected_index_];
}

bool WnnConversionSession::select(std::size_t candidate_index) {
  require_active();
  if (candidate_index >= result_.candidates.size()) {
    throw WnnSessionError("WNN session candidate is out of range");
  }
  if (candidate_index == selected_index_) {
    return false;
  }
  preferences_.remember(input_, result_, candidate_index);
  selected_index_ = candidate_index;
  return true;
}

bool WnnConversionSession::cycle_next() {
  require_active();
  const std::size_t next = (selected_index_ + 1U) % result_.candidates.size();
  return select(next);
}

bool WnnConversionSession::cycle_previous() {
  require_active();
  const std::size_t previous = selected_index_ == 0
                                   ? result_.candidates.size() - 1U
                                   : selected_index_ - 1U;
  return select(previous);
}

JwpText WnnConversionSession::accept() {
  require_active();
  JwpText accepted = selected_candidate().text;
  clear();
  return accepted;
}

JwpText WnnConversionSession::cancel() {
  require_active();
  JwpText cancelled = input_;
  clear();
  return cancelled;
}

void WnnConversionSession::require_active() const {
  if (!active_) {
    throw WnnSessionError("WNN conversion session is not active");
  }
}

}  // namespace jwpqt::core
