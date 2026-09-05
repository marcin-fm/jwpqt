#include "jwpqt/core/jwp_document_history.h"

#include <utility>

namespace jwpqt::core {

JwpDocumentHistory::JwpDocumentHistory(std::size_t max_entries)
    : max_entries_(max_entries) {
  if (max_entries < kMinimumMaxEntries ||
      max_entries > kMaximumMaxEntries) {
    throw JwpDocumentHistoryError("history depth must be between 3 and 1000");
  }
}

std::size_t JwpDocumentHistory::max_entries() const noexcept {
  return max_entries_;
}

std::size_t JwpDocumentHistory::undo_depth() const noexcept {
  return undo_entries_.size();
}

std::size_t JwpDocumentHistory::redo_depth() const noexcept {
  return redo_entries_.size();
}

bool JwpDocumentHistory::can_undo() const noexcept {
  return !undo_entries_.empty();
}

bool JwpDocumentHistory::can_redo() const noexcept {
  return !redo_entries_.empty();
}

bool JwpDocumentHistory::transaction_active() const noexcept {
  return transaction_start_.has_value();
}

std::uint64_t JwpDocumentHistory::generation() const noexcept {
  return generation_;
}

void JwpDocumentHistory::require_caret(const JwpDocumentModel& model,
                                       JwpPosition caret) {
  if (!model.valid_position(caret)) {
    throw JwpDocumentHistoryError("history caret is out of range");
  }
}

void JwpDocumentHistory::require_document(const JwpDocumentModel& model,
                                          const JwpDocument& expected) {
  if (model.document() != expected) {
    throw JwpDocumentHistoryError(
        "document changed outside the recorded history");
  }
}

void JwpDocumentHistory::require_current_document(
    const JwpDocumentModel& model) const {
  if (!undo_entries_.empty()) {
    require_document(model, undo_entries_.back().after.document);
  } else if (!redo_entries_.empty()) {
    require_document(model, redo_entries_.back().before.document);
  }
}

void JwpDocumentHistory::begin(const JwpDocumentModel& model,
                               JwpPosition caret) {
  if (transaction_start_) {
    throw JwpDocumentHistoryError("history transaction is already active");
  }
  require_current_document(model);
  require_caret(model, caret);
  transaction_start_.emplace(State{model.document(), caret});
  ++generation_;
}

bool JwpDocumentHistory::commit(const JwpDocumentModel& model,
                                JwpPosition caret, JwpHistoryKind kind) {
  if (!transaction_start_) {
    throw JwpDocumentHistoryError("no history transaction is active");
  }
  require_caret(model, caret);

  State after{model.document(), caret};
  if (transaction_start_->document == after.document) {
    transaction_start_.reset();
    break_coalescing();
    ++generation_;
    return false;
  }

  const bool coalesce =
      kind != JwpHistoryKind::kNone && may_coalesce_ &&
      !undo_entries_.empty() && undo_entries_.back().kind == kind &&
      undo_entries_.back().after.document == transaction_start_->document &&
      undo_entries_.back().after.caret == transaction_start_->caret;

  if (coalesce) {
    undo_entries_.back().after = std::move(after);
  } else {
    undo_entries_.push_back(
        Entry{*transaction_start_, std::move(after), kind});
    if (undo_entries_.size() > max_entries_) {
      undo_entries_.erase(undo_entries_.begin());
    }
  }

  redo_entries_.clear();
  transaction_start_.reset();
  may_coalesce_ = kind != JwpHistoryKind::kNone;
  ++generation_;
  return true;
}

void JwpDocumentHistory::abandon_unchanged(
    const JwpDocumentModel& model) {
  if (!transaction_start_) {
    throw JwpDocumentHistoryError("no history transaction is active");
  }
  require_document(model, transaction_start_->document);
  transaction_start_.reset();
  ++generation_;
}

void JwpDocumentHistory::cancel() noexcept {
  transaction_start_.reset();
  break_coalescing();
  ++generation_;
}

void JwpDocumentHistory::break_coalescing() noexcept {
  may_coalesce_ = false;
}

void JwpDocumentHistory::clear() noexcept {
  undo_entries_.clear();
  redo_entries_.clear();
  transaction_start_.reset();
  break_coalescing();
  ++generation_;
}

bool JwpDocumentHistory::undo(JwpDocumentModel& model, JwpPosition& caret) {
  if (transaction_start_) {
    throw JwpDocumentHistoryError(
        "cannot undo during an active history transaction");
  }
  if (undo_entries_.empty()) {
    return false;
  }

  require_document(model, undo_entries_.back().after.document);
  Entry entry = undo_entries_.back();
  JwpDocumentModel restored(entry.before.document);
  redo_entries_.push_back(entry);
  undo_entries_.pop_back();
  model = std::move(restored);
  caret = entry.before.caret;
  break_coalescing();
  ++generation_;
  return true;
}

bool JwpDocumentHistory::redo(JwpDocumentModel& model, JwpPosition& caret) {
  if (transaction_start_) {
    throw JwpDocumentHistoryError(
        "cannot redo during an active history transaction");
  }
  if (redo_entries_.empty()) {
    return false;
  }

  require_document(model, redo_entries_.back().before.document);
  Entry entry = redo_entries_.back();
  JwpDocumentModel restored(entry.after.document);
  undo_entries_.push_back(entry);
  redo_entries_.pop_back();
  model = std::move(restored);
  caret = entry.after.caret;
  break_coalescing();
  ++generation_;
  return true;
}

}  // namespace jwpqt::core
