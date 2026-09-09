#pragma once

#include "jwpqt/core/jwp_document_model.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace jwpqt::core {

enum class JwpHistoryKind {
  kNone,
  kTyping,
  kDeletion,
};

class JwpDocumentHistoryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class JwpDocumentHistory {
 public:
  static constexpr std::size_t kDefaultMaxEntries = 50;
  static constexpr std::size_t kMinimumMaxEntries = 3;
  static constexpr std::size_t kMaximumMaxEntries = 1000;

  explicit JwpDocumentHistory(
      std::size_t max_entries = kDefaultMaxEntries);

  std::size_t max_entries() const noexcept;
  void set_max_entries(std::size_t max_entries);
  std::size_t undo_depth() const noexcept;
  std::size_t redo_depth() const noexcept;
  bool can_undo() const noexcept;
  bool can_redo() const noexcept;
  bool transaction_active() const noexcept;
  std::uint64_t generation() const noexcept;

  void begin(const JwpDocumentModel& model, JwpPosition caret);
  bool commit(const JwpDocumentModel& model, JwpPosition caret,
              JwpHistoryKind kind = JwpHistoryKind::kNone);
  void abandon_unchanged(const JwpDocumentModel& model);
  void cancel() noexcept;
  void break_coalescing() noexcept;
  void clear() noexcept;

  bool undo(JwpDocumentModel& model, JwpPosition& caret);
  bool redo(JwpDocumentModel& model, JwpPosition& caret);

 private:
  struct State {
    JwpDocument document;
    JwpPosition caret;
  };

  struct Entry {
    State before;
    State after;
    JwpHistoryKind kind = JwpHistoryKind::kNone;
  };

  static void require_caret(const JwpDocumentModel& model,
                            JwpPosition caret);
  static void require_document(const JwpDocumentModel& model,
                               const JwpDocument& expected);
  void require_current_document(const JwpDocumentModel& model) const;

  std::size_t max_entries_;
  std::vector<Entry> undo_entries_;
  std::vector<Entry> redo_entries_;
  std::optional<State> transaction_start_;
  bool may_coalesce_ = false;
  std::uint64_t generation_ = 0;
};

}  // namespace jwpqt::core
