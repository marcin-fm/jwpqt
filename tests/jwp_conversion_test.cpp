// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_conversion.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using jwpqt::core::JwpConversionError;
using jwpqt::core::JwpConversionTransaction;
using jwpqt::core::JwpDocument;
using jwpqt::core::JwpDocumentHistory;
using jwpqt::core::JwpDocumentModel;
using jwpqt::core::JwpParagraph;
using jwpqt::core::JwpPosition;
using jwpqt::core::JwpRange;
using jwpqt::core::JwpText;
using jwpqt::core::JisCode;
using jwpqt::core::WnnConversionSession;
using jwpqt::core::WnnDictionary;
using jwpqt::core::WnnPreferences;

static_assert(!std::is_copy_constructible_v<JwpConversionTransaction>);
static_assert(!std::is_move_constructible_v<JwpConversionTransaction>);

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Function>
void require_conversion_error(Function&& function, std::string_view message) {
  try {
    function();
  } catch (const JwpConversionError&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

void append_u32_le(std::string& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
  bytes.push_back(static_cast<char>((value >> 16U) & 0xffU));
  bytes.push_back(static_cast<char>((value >> 24U) & 0xffU));
}

WnnDictionary dictionary() {
  const std::string data = std::string("\xa2", 1) +
                           "*\xb0\xa1/\xb0\xa2\xb0\xa3\n";
  std::string index({static_cast<char>(0xa2), static_cast<char>(0x80),
                     static_cast<char>(0x80), 'w'});
  append_u32_le(index, 0);
  return WnnDictionary::parse(index, data);
}

JwpDocumentModel document_model(JisCode kana = 0x2422) {
  JwpDocument document;
  JwpParagraph paragraph;
  paragraph.text = {'X', kana, 'Y'};
  paragraph.line_spacing = 135;
  document.paragraphs.push_back(std::move(paragraph));
  return JwpDocumentModel(std::move(document));
}

JwpText text(const JwpDocumentModel& model) {
  return model.paragraph(0).text;
}

void test_cycle_accept_is_one_undo_transaction() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(3);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model();
  JwpDocumentHistory history;
  JwpConversionTransaction conversion(model, history, session);

  require(conversion.begin({{0, 1}, {0, 2}}, {0, 2}),
          "Conversion did not begin");
  require(text(model) == JwpText({'X', 0x3021, 'Y'}) &&
              conversion.range().begin == JwpPosition{0, 1} &&
              conversion.range().end == JwpPosition{0, 2} &&
              conversion.caret() == JwpPosition{0, 2},
          "Initial candidate did not replace the selected kana");
  require(history.transaction_active() && history.undo_depth() == 0,
          "Initial candidate prematurely committed history");

  require(conversion.cycle_next(), "Candidate did not cycle forward");
  require(text(model) == JwpText({'X', 0x3022, 0x3023, 'Y'}) &&
              conversion.range().end == JwpPosition{0, 3} &&
              conversion.caret() == JwpPosition{0, 3},
          "Cycled candidate did not update range and caret");
  require(conversion.accept() && !conversion.active(),
          "Accepted conversion did not commit");
  require(history.undo_depth() == 1 && !history.transaction_active(),
          "Accepted conversion was not one history entry");

  JwpPosition caret = {0, 3};
  require(history.undo(model, caret) &&
              text(model) == JwpText({'X', 0x2422, 'Y'}) &&
              caret == JwpPosition{0, 2},
          "Conversion undo did not restore original kana and caret");
  require(history.redo(model, caret) &&
              text(model) == JwpText({'X', 0x3022, 0x3023, 'Y'}) &&
              caret == JwpPosition{0, 3},
          "Conversion redo did not restore selected candidate and caret");
}

void test_internal_rollback_restores_exact_document_and_caret() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model();
  const JwpDocument original = model.document();
  JwpDocumentHistory history;
  JwpConversionTransaction conversion(model, history, session);

  require(conversion.begin({{0, 1}, {0, 2}}, {0, 1}),
          "Start-oriented conversion did not begin");
  require(conversion.caret() == JwpPosition{0, 1},
          "Conversion did not preserve start-oriented caret");
  conversion.cycle_next();
  require(conversion.caret() == JwpPosition{0, 1},
          "Cycling moved a start-oriented caret");
  const JwpPosition caret = conversion.rollback();
  require(caret == JwpPosition{0, 1} && model.document() == original &&
              !conversion.active() &&
              !history.transaction_active() && !history.can_undo(),
          "Rollback did not restore the exact original state and caret");
}

void test_end_oriented_rollback_returns_original_caret() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model();
  JwpDocumentHistory history;
  JwpConversionTransaction conversion(model, history, session);

  require(conversion.begin({{0, 1}, {0, 2}}, {0, 2}) &&
              conversion.cycle_next(),
          "End-oriented conversion did not cycle");
  require(conversion.caret() == JwpPosition{0, 3},
          "Longer candidate did not move the live caret");
  require(conversion.rollback() == JwpPosition{0, 2},
          "Rollback did not return the original end caret");
}

void test_destructor_rolls_back_and_releases_shared_state() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model();
  const JwpDocument original = model.document();
  JwpDocumentHistory history;
  {
    JwpConversionTransaction conversion(model, history, session);
    require(conversion.begin({{0, 1}, {0, 2}}, {0, 2}),
            "Scoped conversion did not begin");
  }
  require(model.document() == original && !history.transaction_active() &&
              !session.active(),
          "Destructor did not roll back and release shared state");
}

void test_external_mutation_is_not_committed_or_overwritten() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model();
  JwpDocumentHistory history;
  {
    JwpConversionTransaction conversion(model, history, session);
    require(conversion.begin({{0, 1}, {0, 2}}, {0, 2}),
            "Conversion did not begin");
    model.insert({0, 3}, JwpText{'Z'});
    const JwpDocument externally_changed = model.document();
    require_conversion_error([&] { conversion.cycle_next(); },
                             "Cycle accepted external document mutation");
    require_conversion_error([&] { conversion.accept(); },
                             "Accept committed external document mutation");
    require_conversion_error([&] { conversion.rollback(); },
                             "Rollback overwrote external document mutation");
    require(model.document() == externally_changed,
            "Failed lifecycle operation changed external mutation");
  }
  require(text(model) == JwpText({'X', 0x3021, 'Y', 'Z'}) &&
              !history.transaction_active() && !session.active(),
          "Destructor overwrote external mutation or stranded shared state");
}

void test_external_collaborator_changes_are_rejected() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model();
  const JwpDocument original = model.document();
  JwpDocumentHistory history;
  {
    JwpConversionTransaction conversion(model, history, session);
    require(conversion.begin({{0, 1}, {0, 2}}, {0, 2}),
            "Conversion did not begin before session mutation");
    session.clear();
    require(session.begin(JwpText{0x2422}),
            "Replacement session did not begin");
    const JwpDocument converted = model.document();
    require_conversion_error([&] { conversion.accept(); },
                             "Conversion accepted a replacement session");
    require(model.document() == converted,
            "Rejected replacement session changed the document");
  }
  require(model.document() != original && !history.transaction_active() &&
              session.active(),
          "Teardown overwrote document or replacement session state");
  session.clear();
  model = JwpDocumentModel(original);

  WnnConversionSession second_session(system, preferences);
  {
    JwpConversionTransaction conversion(model, history, second_session);
    require(conversion.begin({{0, 1}, {0, 2}}, {0, 2}),
            "Conversion did not begin before history mutation");
    const JwpPosition converted_caret = conversion.caret();
    history.cancel();
    history.begin(model, converted_caret);
    const JwpDocument converted = model.document();
    require_conversion_error([&] { conversion.cycle_next(); },
                             "Conversion used replacement history");
    require(model.document() == converted,
            "Rejected replacement history changed the document");
  }
  require(model.document() != original && history.transaction_active() &&
              !second_session.active(),
          "Teardown overwrote document or replacement history state");
  history.cancel();
}

void test_failed_conversion_preserves_typing_coalescing() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model;
  JwpDocumentHistory history;
  JwpPosition caret{};

  history.begin(model, caret);
  caret = model.insert(caret, JwpText{0x2424});
  require(history.commit(model, caret,
                         jwpqt::core::JwpHistoryKind::kTyping),
          "Initial typing did not commit");
  JwpConversionTransaction conversion(model, history, session);
  require(!conversion.begin({{0, 0}, {0, 1}}, caret),
          "Unknown kana unexpectedly began conversion");
  history.begin(model, caret);
  caret = model.insert(caret, JwpText{'A'});
  require(history.commit(model, caret,
                         jwpqt::core::JwpHistoryKind::kTyping) &&
              history.undo_depth() == 1,
          "Failed conversion broke typing coalescing");
  require(history.undo(model, caret) && text(model).empty(),
          "Coalesced typing did not undo as one entry");
}

void test_no_match_and_invalid_ranges() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model(0x2424);
  JwpDocumentHistory history;
  JwpConversionTransaction conversion(model, history, session);
  const JwpDocument original = model.document();

  require(!conversion.begin({{0, 1}, {0, 2}}, {0, 2}),
          "Unknown kana unexpectedly began conversion");
  require(model.document() == original && !history.transaction_active(),
          "No-match conversion changed document or history");
  require_conversion_error([&] { conversion.begin({{0, 1}, {0, 1}}, {0, 1}); },
                           "Empty conversion range was accepted");
  require_conversion_error([&] { conversion.begin({{0, 1}, {0, 2}}, {0, 0}); },
                           "Caret outside conversion endpoints was accepted");
  require_conversion_error([&] { conversion.accept(); },
                           "Inactive conversion was accepted");
}

void test_selection_validation_and_previous_wrap() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  JwpDocumentModel model = document_model();
  JwpDocumentHistory history;
  JwpConversionTransaction conversion(model, history, session);
  require(conversion.begin({{0, 1}, {0, 2}}, {0, 2}),
          "Conversion did not begin");

  require(conversion.cycle_previous() && conversion.selected_index() == 2 &&
              text(model) == JwpText({'X', 0x2422, 'Y'}),
          "Previous did not wrap to original kana");
  require(!conversion.select(2),
          "Selecting the active candidate reported a change");
  require_conversion_error([&] { conversion.select(3); },
                           "Out-of-range candidate was accepted");
  require(conversion.result().candidates.size() == 3,
          "Conversion did not expose session candidates");
  require(!conversion.accept(),
          "No-op original-kana conversion created history");
  require(!history.can_undo(),
          "No-op original-kana conversion left an undo entry");
}

void run_tests() {
  test_cycle_accept_is_one_undo_transaction();
  test_internal_rollback_restores_exact_document_and_caret();
  test_end_oriented_rollback_returns_original_caret();
  test_destructor_rolls_back_and_releases_shared_state();
  test_external_mutation_is_not_committed_or_overwritten();
  test_external_collaborator_changes_are_rejected();
  test_failed_conversion_preserves_typing_coalescing();
  test_no_match_and_invalid_ranges();
  test_selection_validation_and_previous_wrap();
}

}  // namespace

int main() {
  try {
    run_tests();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "JWP conversion transaction tests passed\n";
  return EXIT_SUCCESS;
}
