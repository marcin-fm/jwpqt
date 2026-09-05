#include "jwpqt/core/jwp_document_history.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <cmath>
#include <string_view>
#include <utility>

namespace {

using jwpqt::core::JwpDocument;
using jwpqt::core::JwpDocumentHistory;
using jwpqt::core::JwpDocumentHistoryError;
using jwpqt::core::JwpDocumentModel;
using jwpqt::core::JwpHistoryKind;
using jwpqt::core::JwpParagraph;
using jwpqt::core::JwpPosition;
using jwpqt::core::JwpText;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

template <typename Function>
void expect_error(Function function, std::string_view message) {
  try {
    function();
  } catch (const JwpDocumentHistoryError&) {
    return;
  }
  std::cerr << "FAIL: expected JwpDocumentHistoryError for " << message
            << '\n';
  std::exit(1);
}

JwpDocumentModel model_with_text(JwpText text = {}) {
  JwpDocument document;
  JwpParagraph paragraph;
  paragraph.text = std::move(text);
  document.paragraphs.push_back(std::move(paragraph));
  return JwpDocumentModel(std::move(document));
}

JwpText text_of(const JwpDocumentModel& model) {
  return model.paragraph(0).text;
}

void insert_transaction(JwpDocumentHistory& history, JwpDocumentModel& model,
                        JwpPosition& caret, jwpqt::core::JisCode code,
                        JwpHistoryKind kind = JwpHistoryKind::kNone) {
  history.begin(model, caret);
  caret = model.insert(caret, JwpText{code});
  expect(history.commit(model, caret, kind), "insert transaction commits");
}

void test_defaults_and_limits() {
  JwpDocumentHistory history;
  expect(history.max_entries() == 50, "legacy default history depth");
  expect(!history.can_undo() && !history.can_redo(), "empty history state");
  expect(!history.transaction_active(), "no default transaction");

  expect_error([] { JwpDocumentHistory history(2); }, "depth below minimum");
  expect_error([] { JwpDocumentHistory history(1001); },
               "depth above maximum");
  JwpDocumentHistory minimum(3);
  JwpDocumentHistory maximum(1000);
  expect(minimum.max_entries() == 3 && maximum.max_entries() == 1000,
         "history limit endpoints");
}

void test_grouped_transaction_restores_document_and_caret() {
  JwpDocumentModel model = model_with_text({'A', 'D'});
  JwpDocumentHistory history;
  JwpPosition caret{0, 1};

  history.begin(model, caret);
  caret = model.insert(caret, JwpText{'B', 'C'});
  caret = model.split_paragraph(caret);
  model.set_page_break(1, true);
  expect(history.commit(model, {1, 0}), "grouped transaction commit");
  caret = {1, 0};
  expect(history.undo_depth() == 1, "grouped transaction is one entry");

  expect(history.undo(model, caret), "grouped transaction undo");
  expect(model.paragraph_count() == 1 && text_of(model) == JwpText{'A', 'D'},
         "grouped undo restores document");
  expect(caret == JwpPosition{0, 1}, "grouped undo restores caret");

  expect(history.redo(model, caret), "grouped transaction redo");
  expect(model.paragraph_count() == 2 && model.paragraph(0).text ==
                                               JwpText({'A', 'B', 'C'}) &&
             model.paragraph(1).page_break,
         "grouped redo restores document");
  expect(caret == JwpPosition{1, 0}, "grouped redo restores caret");
}

void test_typing_coalesces_until_broken() {
  JwpDocumentModel model = model_with_text();
  JwpDocumentHistory history;
  JwpPosition caret{};

  insert_transaction(history, model, caret, 'A', JwpHistoryKind::kTyping);
  insert_transaction(history, model, caret, 'B', JwpHistoryKind::kTyping);
  expect(history.undo_depth() == 1, "typing coalesces");
  expect(history.undo(model, caret) && text_of(model).empty(),
         "coalesced typing undo");
  expect(caret == JwpPosition{}, "coalesced typing restores first caret");
  expect(history.redo(model, caret) && text_of(model) == JwpText({'A', 'B'}),
         "coalesced typing redo");

  history.break_coalescing();
  insert_transaction(history, model, caret, 'C', JwpHistoryKind::kTyping);
  expect(history.undo_depth() == 2, "navigation boundary breaks typing run");
  expect(history.undo(model, caret) && text_of(model) == JwpText({'A', 'B'}),
         "broken typing run undoes separately");
}

void test_unchanged_abandon_preserves_typing_coalescing() {
  JwpDocumentModel model = model_with_text();
  JwpDocumentHistory history;
  JwpPosition caret{};

  insert_transaction(history, model, caret, 'A', JwpHistoryKind::kTyping);
  history.begin(model, caret);
  history.abandon_unchanged(model);
  insert_transaction(history, model, caret, 'B', JwpHistoryKind::kTyping);
  expect(history.undo_depth() == 1,
         "unchanged abandoned transaction preserves typing run");

  history.begin(model, caret);
  model.insert(caret, JwpText{'C'});
  expect_error([&] { history.abandon_unchanged(model); },
               "abandon after document mutation");
  history.cancel();
}

void test_deletions_coalesce_but_not_with_typing() {
  JwpDocumentModel model = model_with_text({'A', 'B', 'C'});
  JwpDocumentHistory history;
  JwpPosition caret{0, 3};

  history.begin(model, caret);
  caret = model.erase({{0, 2}, caret});
  expect(history.commit(model, caret, JwpHistoryKind::kDeletion),
         "backspace commit");
  history.begin(model, caret);
  caret = model.erase({{0, 1}, caret});
  expect(history.commit(model, caret, JwpHistoryKind::kDeletion),
         "delete commit");
  expect(history.undo_depth() == 1, "backspace and delete coalesce");
  insert_transaction(history, model, caret, 'D', JwpHistoryKind::kTyping);
  expect(history.undo_depth() == 2, "typing does not join deletion run");
  expect(history.undo(model, caret) && text_of(model) == JwpText{'A'},
         "typing undoes separately from deletion run");
  expect(history.undo(model, caret) && text_of(model) == JwpText({'A', 'B', 'C'}),
         "coalesced deletions undo together");
}

void test_new_edit_invalidates_redo() {
  JwpDocumentModel model = model_with_text();
  JwpDocumentHistory history;
  JwpPosition caret{};
  insert_transaction(history, model, caret, 'A');
  expect(history.undo(model, caret) && history.can_redo(), "redo available");
  insert_transaction(history, model, caret, 'B');
  expect(!history.can_redo(), "new edit clears redo");
}

void test_noop_cancel_and_clear() {
  JwpDocumentModel model = model_with_text({'A'});
  JwpDocumentHistory history;
  JwpPosition caret{0, 1};

  history.begin(model, caret);
  expect(!history.commit(model, caret, JwpHistoryKind::kTyping),
         "no-op transaction is not recorded");
  history.begin(model, caret);
  history.cancel();
  expect(!history.transaction_active() && !history.can_undo(),
         "cancel discards transaction");

  insert_transaction(history, model, caret, 'B');
  history.clear();
  expect(!history.can_undo() && !history.can_redo() &&
             !history.transaction_active(),
         "clear resets complete history");
}

void test_noop_with_nan_margin_is_not_recorded() {
  JwpDocument document;
  document.margins[0] = std::numeric_limits<float>::quiet_NaN();
  document.paragraphs.push_back(JwpParagraph{});
  JwpDocumentModel model(std::move(document));
  JwpDocumentHistory history;

  history.begin(model, {});
  expect(!history.commit(model, {}), "NaN margin snapshot compares stably");
  expect(!history.can_undo(), "NaN no-op creates no history entry");
}

void test_depth_evicts_oldest_complete_entry() {
  JwpDocumentModel model = model_with_text();
  JwpDocumentHistory history(3);
  JwpPosition caret{};
  insert_transaction(history, model, caret, 'A');
  insert_transaction(history, model, caret, 'B');
  insert_transaction(history, model, caret, 'C');
  insert_transaction(history, model, caret, 'D');
  expect(history.undo_depth() == 3, "history depth is bounded");
  expect(history.undo(model, caret), "undo D");
  expect(history.undo(model, caret), "undo C");
  expect(history.undo(model, caret), "undo B");
  expect(text_of(model) == JwpText{'A'}, "oldest complete entry was evicted");
  expect(!history.undo(model, caret), "no evicted undo remains");
}

void test_invalid_lifecycle_and_external_mutation() {
  JwpDocumentModel model = model_with_text();
  JwpDocumentHistory history;
  JwpPosition caret{};

  expect_error([&] { history.begin(model, {1, 0}); }, "invalid begin caret");
  expect_error([&] { history.commit(model, caret); }, "commit without begin");
  history.begin(model, caret);
  expect_error([&] { history.begin(model, caret); }, "nested begin");
  expect_error([&] { history.undo(model, caret); }, "undo during transaction");
  expect_error([&] { history.redo(model, caret); }, "redo during transaction");
  history.cancel();

  insert_transaction(history, model, caret, 'A');
  model.insert(caret, JwpText{'B'});
  expect_error([&] { history.begin(model, caret); },
               "begin after external mutation");
  expect_error([&] { history.undo(model, caret); }, "external mutation");
  expect(history.undo_depth() == 1 && history.redo_depth() == 0,
         "failed undo preserves stacks");
}

}  // namespace

int main() {
  test_defaults_and_limits();
  test_grouped_transaction_restores_document_and_caret();
  test_typing_coalesces_until_broken();
  test_unchanged_abandon_preserves_typing_coalescing();
  test_deletions_coalesce_but_not_with_typing();
  test_new_edit_invalidates_redo();
  test_noop_cancel_and_clear();
  test_noop_with_nan_margin_is_not_recorded();
  test_depth_evicts_oldest_complete_entry();
  test_invalid_lifecycle_and_external_mutation();
  return 0;
}
