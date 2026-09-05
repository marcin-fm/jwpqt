// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_session.h"

#include <algorithm>
#include <utility>

namespace jwpqt::core {

WnnPreparedConversion::WnnPreparedConversion(
    WnnPreparedConversion&& other) noexcept
    : input_(std::move(other.input_)),
      result_(std::move(other.result_)),
      selected_index_(other.selected_index_),
      valid_(other.valid_) {
  other.selected_index_ = 0;
  other.valid_ = false;
}

WnnPreparedConversion& WnnPreparedConversion::operator=(
    WnnPreparedConversion&& other) noexcept {
  if (this != &other) {
    input_ = std::move(other.input_);
    result_ = std::move(other.result_);
    selected_index_ = other.selected_index_;
    valid_ = other.valid_;
    other.selected_index_ = 0;
    other.valid_ = false;
  }
  return *this;
}

const JwpText& WnnPreparedConversion::input() const {
  if (!valid_) {
    throw WnnSessionError("prepared WNN conversion is no longer valid");
  }
  return input_;
}

const WnnLookupResult& WnnPreparedConversion::result() const {
  if (!valid_) {
    throw WnnSessionError("prepared WNN conversion is no longer valid");
  }
  return result_;
}

std::size_t WnnPreparedConversion::selected_index() const {
  if (!valid_) {
    throw WnnSessionError("prepared WNN conversion is no longer valid");
  }
  return selected_index_;
}

const WnnCandidate& WnnPreparedConversion::selected_candidate() const {
  if (!valid_ || selected_index_ >= result_.candidates.size()) {
    throw WnnSessionError("prepared WNN candidate is unavailable");
  }
  return result_.candidates[selected_index_];
}

WnnConversionSession::WnnConversionSession(
    const WnnDictionary& system_dictionary, WnnPreferences& preferences,
    const std::vector<WnnRecord>* user_records,
    std::size_t maximum_output_cells)
    : system_dictionary_(system_dictionary),
      preferences_(preferences),
      user_records_(user_records),
      maximum_output_cells_(maximum_output_cells) {}

bool WnnConversionSession::begin(const JwpText& input) {
  std::optional<WnnPreparedConversion> prepared = prepare(input);
  if (!prepared) {
    clear();
    return false;
  }
  activate(std::move(*prepared));
  return true;
}

std::optional<WnnPreparedConversion> WnnConversionSession::prepare(
    const JwpText& input) const {
  WnnPreparedConversion prepared;
  prepared.input_ = input;
  prepared.result_ = lookup_wnn_candidates(
      system_dictionary_, input, user_records_, maximum_output_cells_);
  if (prepared.result_.candidates.empty()) {
    return std::nullopt;
  }
  const std::optional<std::size_t> preferred =
      preferences_.preferred_candidate(input, prepared.result_);
  if (!preferred || *preferred >= prepared.result_.candidates.size()) {
    throw WnnSessionError("WNN preferences returned an invalid candidate");
  }
  prepared.selected_index_ = *preferred;
  prepared.valid_ = true;
  return prepared;
}

WnnAutomaticPreparation WnnConversionSession::prepare_automatic(
    const JwpText& input) const {
  WnnAutomaticPreparation automatic;
  if (input.empty()) {
    return automatic;
  }

  std::optional<WnnPreparedConversion> prepared;
  if (input.size() <= kWnnMaximumKeySize) {
    prepared = prepare(input);
    if (prepared) {
      automatic.matched_length = input.size();
      if (prepared->result().can_extend) {
        automatic.wait_for_more = true;
      } else {
        automatic.conversion.emplace(std::move(*prepared));
      }
      return automatic;
    }
  }

  const std::size_t first_prefix =
      std::min(input.size() - 1U, kWnnMaximumKeySize);
  for (std::size_t length = first_prefix; length > 0; --length) {
    const JwpText prefix(input.begin(), input.begin() +
                                            static_cast<JwpText::difference_type>(
                                                length));
    prepared = prepare(prefix);
    if (prepared) {
      automatic.matched_length = length;
      automatic.conversion.emplace(std::move(*prepared));
      return automatic;
    }
  }
  return automatic;
}

void WnnConversionSession::activate(WnnPreparedConversion prepared) {
  if (!prepared.valid_ || prepared.result_.candidates.empty() ||
      prepared.selected_index_ >= prepared.result_.candidates.size()) {
    throw WnnSessionError("prepared WNN conversion is invalid");
  }
  preferences_.remember(prepared.input_, prepared.result_,
                        prepared.selected_index_);
  input_.swap(prepared.input_);
  result_.candidates.swap(prepared.result_.candidates);
  std::swap(result_.can_extend, prepared.result_.can_extend);
  selected_index_ = prepared.selected_index_;
  active_ = true;
  ++generation_;
  prepared.valid_ = false;
}

void WnnConversionSession::clear() noexcept {
  const bool was_active = active_;
  JwpText{}.swap(input_);
  std::vector<WnnCandidate>{}.swap(result_.candidates);
  result_.can_extend = false;
  selected_index_ = 0;
  active_ = false;
  if (was_active) {
    ++generation_;
  }
}

bool WnnConversionSession::active() const noexcept { return active_; }

std::uint64_t WnnConversionSession::generation() const noexcept {
  return generation_;
}

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
  ++generation_;
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
