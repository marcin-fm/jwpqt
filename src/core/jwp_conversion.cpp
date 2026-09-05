// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_conversion.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace jwpqt::core {
namespace {

JwpText selected_text(const JwpDocumentModel& model, JwpRange range) {
  if (!model.valid_position(range.begin) || !model.valid_position(range.end) ||
      range.end < range.begin) {
    throw JwpConversionError("conversion range is out of bounds");
  }
  if (range.begin.paragraph != range.end.paragraph) {
    throw JwpConversionError("conversion range crosses paragraphs");
  }
  if (range.begin == range.end) {
    throw JwpConversionError("conversion range is empty");
  }

  const JwpText& text = model.paragraph(range.begin.paragraph).text;
  return JwpText(
      text.begin() + static_cast<JwpText::difference_type>(range.begin.offset),
      text.begin() + static_cast<JwpText::difference_type>(range.end.offset));
}

void validate_caret(const JwpDocumentModel& model, JwpRange range,
                    JwpPosition caret) {
  if (!model.valid_position(caret)) {
    throw JwpConversionError("conversion caret is out of bounds");
  }
  if (caret != range.begin &&
      (caret.paragraph != range.end.paragraph ||
       caret.offset < range.end.offset)) {
    throw JwpConversionError(
        "conversion caret is neither at the start nor after the range");
  }
}

}  // namespace

static_assert(std::is_nothrow_move_assignable_v<JwpDocumentModel>);

JwpConversionTransaction::JwpConversionTransaction(
    JwpDocumentModel& model, JwpDocumentHistory& history,
    WnnConversionSession& session)
    : model_(model), history_(history), session_(session) {}

JwpConversionTransaction::~JwpConversionTransaction() noexcept {
  rollback_noexcept();
}

bool JwpConversionTransaction::begin(JwpRange range, JwpPosition caret) {
  if (active_ || session_.active() || history_.transaction_active()) {
    throw JwpConversionError("conversion transaction is already active");
  }
  const JwpText input = selected_text(model_, range);
  validate_caret(model_, range, caret);
  std::optional<WnnPreparedConversion> prepared = session_.prepare(input);
  if (!prepared) {
    return false;
  }
  start_prepared(range, caret, std::move(*prepared));
  return true;
}

void JwpConversionTransaction::begin_prepared(
    JwpRange range, JwpPosition caret, WnnPreparedConversion prepared) {
  if (active_ || session_.active() || history_.transaction_active()) {
    throw JwpConversionError("conversion transaction is already active");
  }
  if (selected_text(model_, range) != prepared.input()) {
    throw JwpConversionError(
        "prepared conversion does not match the document range");
  }
  validate_caret(model_, range, caret);
  start_prepared(range, caret, std::move(prepared));
}

void JwpConversionTransaction::start_prepared(
    JwpRange range, JwpPosition caret, WnnPreparedConversion prepared) {
  caret_at_start_ = caret == range.begin;
  if (!caret_at_start_) {
    if (caret.paragraph != range.end.paragraph ||
        caret.offset < range.end.offset) {
      throw JwpConversionError(
          "conversion caret is neither at the start nor after the range");
    }
    caret_tail_offset_ = caret.offset - range.end.offset;
  } else {
    caret_tail_offset_ = 0;
  }

  JwpDocument original = model_.document();
  JwpDocumentModel next(model_.document());
  const JwpPosition begin = next.erase(range);
  const JwpPosition end = next.insert(begin, prepared.selected_candidate().text);
  const JwpPosition next_caret = adjusted_caret(begin, end);
  if (!next.valid_position(next_caret)) {
    throw JwpConversionError("converted caret is out of bounds");
  }
  JwpDocument expected = next.document();

  history_.begin(model_, caret);
  try {
    session_.activate(std::move(prepared));
    original_document_.emplace(std::move(original));
    expected_document_.emplace(std::move(expected));
    model_ = std::move(next);
    expected_history_generation_ = history_.generation();
    expected_session_generation_ = session_.generation();
    original_caret_ = caret;
    range_ = {begin, end};
    caret_ = next_caret;
    active_ = true;
  } catch (...) {
    history_.abandon_unchanged(model_);
    session_.clear();
    original_document_.reset();
    expected_document_.reset();
    throw;
  }
}

bool JwpConversionTransaction::active() const noexcept { return active_; }

JwpRange JwpConversionTransaction::range() const {
  require_active();
  return range_;
}

JwpPosition JwpConversionTransaction::caret() const {
  require_active();
  return caret_;
}

std::size_t JwpConversionTransaction::selected_index() const {
  require_active();
  return session_.selected_index();
}

const WnnLookupResult& JwpConversionTransaction::result() const {
  require_active();
  return session_.result();
}

bool JwpConversionTransaction::select(std::size_t candidate_index) {
  require_active();
  return replace_with(candidate_index);
}

bool JwpConversionTransaction::cycle_next() {
  require_active();
  const std::size_t next =
      (session_.selected_index() + 1U) % session_.result().candidates.size();
  return replace_with(next);
}

bool JwpConversionTransaction::cycle_previous() {
  require_active();
  const std::size_t previous = session_.selected_index() == 0
                                   ? session_.result().candidates.size() - 1U
                                   : session_.selected_index() - 1U;
  return replace_with(previous);
}

bool JwpConversionTransaction::accept() {
  require_active();
  require_expected_document();
  const bool committed = history_.commit(model_, caret_);
  session_.clear();
  finish();
  return committed;
}

JwpPosition JwpConversionTransaction::rollback() {
  require_active();
  require_expected_document();
  const JwpPosition restored_caret = original_caret_;
  JwpDocumentModel restored(*original_document_);
  model_ = std::move(restored);
  history_.cancel();
  session_.clear();
  finish();
  return restored_caret;
}

void JwpConversionTransaction::require_active() const {
  if (!active_) {
    throw JwpConversionError("conversion transaction is not active");
  }
  if (!history_.transaction_active() || !session_.active() ||
      history_.generation() != expected_history_generation_ ||
      session_.generation() != expected_session_generation_) {
    throw JwpConversionError(
        "conversion collaborators changed outside the transaction");
  }
}

void JwpConversionTransaction::require_expected_document() const {
  if (!expected_document_ || model_.document() != *expected_document_) {
    throw JwpConversionError(
        "document changed outside the conversion transaction");
  }
}

bool JwpConversionTransaction::replace_with(std::size_t candidate_index) {
  require_expected_document();
  if (candidate_index >= session_.result().candidates.size()) {
    throw JwpConversionError("conversion candidate is out of range");
  }
  if (candidate_index == session_.selected_index()) {
    return false;
  }

  JwpDocumentModel next(model_.document());
  const JwpPosition begin = next.erase(range_);
  const JwpPosition end = next.insert(
      begin, session_.result().candidates[candidate_index].text);
  const JwpPosition next_caret = adjusted_caret(begin, end);
  if (!next.valid_position(next_caret)) {
    throw JwpConversionError("converted caret is out of bounds");
  }
  JwpDocument expected = next.document();
  session_.select(candidate_index);
  expected_session_generation_ = session_.generation();
  model_ = std::move(next);
  expected_document_.emplace(std::move(expected));
  range_ = {begin, end};
  caret_ = next_caret;
  return true;
}

JwpPosition JwpConversionTransaction::adjusted_caret(
    JwpPosition begin, JwpPosition end) const {
  if (caret_at_start_) {
    return begin;
  }
  if (caret_tail_offset_ >
      std::numeric_limits<std::size_t>::max() - end.offset) {
    throw JwpConversionError("converted caret offset overflows");
  }
  return {end.paragraph, end.offset + caret_tail_offset_};
}

void JwpConversionTransaction::rollback_noexcept() noexcept {
  if (!active_) {
    return;
  }
  const bool owns_history =
      history_.transaction_active() &&
      history_.generation() == expected_history_generation_;
  const bool owns_session =
      session_.active() &&
      session_.generation() == expected_session_generation_;
  if (owns_history && owns_session && expected_document_ &&
      model_.document() == *expected_document_) {
    try {
      JwpDocumentModel restored(std::move(*original_document_));
      model_ = std::move(restored);
    } catch (...) {
    }
  }
  if (owns_history) {
    history_.cancel();
  }
  if (owns_session) {
    session_.clear();
  }
  finish();
}

void JwpConversionTransaction::finish() noexcept {
  original_document_.reset();
  expected_document_.reset();
  original_caret_ = {};
  range_ = {};
  caret_ = {};
  expected_history_generation_ = 0;
  expected_session_generation_ = 0;
  caret_tail_offset_ = 0;
  caret_at_start_ = false;
  active_ = false;
}

}  // namespace jwpqt::core
