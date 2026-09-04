#include "jwpqt/core/jwp_document_history.h"
#include "jwpqt/core/jwp_search.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

namespace {

using jwpqt::core::JwpDocument;
using jwpqt::core::JwpDocumentHistory;
using jwpqt::core::JwpDocumentModel;
using jwpqt::core::JwpParagraph;
using jwpqt::core::JwpPosition;
using jwpqt::core::JwpRange;
using jwpqt::core::JwpSearchDirection;
using jwpqt::core::JwpSearchError;
using jwpqt::core::JwpSearchOptions;
using jwpqt::core::JwpText;
using jwpqt::core::find_all;
using jwpqt::core::find_next;
using jwpqt::core::replace_range;

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
  } catch (const JwpSearchError&) {
    return;
  }
  std::cerr << "FAIL: expected JwpSearchError for " << message << '\n';
  std::exit(1);
}

JwpDocumentModel model_with_paragraphs(
    std::initializer_list<JwpText> paragraphs) {
  JwpDocument document;
  for (const JwpText& text : paragraphs) {
    JwpParagraph paragraph;
    paragraph.text = text;
    document.paragraphs.push_back(std::move(paragraph));
  }
  return JwpDocumentModel(std::move(document));
}

void expect_match(const jwpqt::core::JwpSearchResult& result,
                  JwpRange expected, std::string_view message) {
  expect(result.match.has_value(), message);
  expect(result.match->begin == expected.begin &&
             result.match->end == expected.end,
         message);
}

void test_forward_excludes_start_and_matches_within_paragraphs() {
  JwpDocumentModel model =
      model_with_paragraphs({{'a', 'b', 'a', 'b'}, {'a', 'b'}});

  auto result = find_next(model, JwpText{'a', 'b'}, {0, 0});
  expect_match(result, {{0, 2}, {0, 4}}, "forward overlapping candidate");
  result = find_next(model, JwpText{'a', 'b'}, result.match->begin);
  expect_match(result, {{1, 0}, {1, 2}}, "forward next paragraph");
  expect(!result.wrapped, "ordinary forward search does not wrap");
  expect(!find_next(model, JwpText{'a', 'b'}, {1, 0}).match,
         "forward start is excluded");

  expect(!find_next(model, JwpText{'b', 'a'}, {0, 1}).match,
         "search does not cross paragraph boundaries");
}

void test_forward_wrap_stops_before_start() {
  JwpDocumentModel model = model_with_paragraphs({{'x', 'a', 'x', 'a'}});
  JwpSearchOptions options;
  options.wrap = true;

  const auto result = find_next(model, JwpText{'a'}, {0, 3}, options);
  expect_match(result, {{0, 1}, {0, 2}}, "forward wrapped match");
  expect(result.wrapped, "forward wrap is reported");

  expect(!find_next(model, JwpText{'a'}, {0, 1}, options).wrapped,
         "later forward match precedes wrap");

  JwpDocumentModel only_start = model_with_paragraphs({{'A'}});
  expect(!find_next(only_start, JwpText{'A'}, {0, 0}, options).match,
         "forward wrap never repeats starting candidate");
  JwpDocumentModel end_start = model_with_paragraphs({{'B', 'A'}});
  const auto end_result = find_next(end_start, JwpText{'A'}, {0, 2}, options);
  expect_match(end_result, {{0, 1}, {0, 2}},
               "forward end offset wraps to preceding match");
  expect(end_result.wrapped, "forward end offset reports wrap");
}

void test_backward_and_backward_wrap() {
  JwpDocumentModel model =
      model_with_paragraphs({{'a', 'b'}, {'x'}, {'a', 'b', 'a', 'b'}});
  JwpSearchOptions options;
  options.direction = JwpSearchDirection::kBackward;

  auto result = find_next(model, JwpText{'a', 'b'}, {2, 2}, options);
  expect_match(result, {{2, 0}, {2, 2}}, "backward excludes start");
  result = find_next(model, JwpText{'a', 'b'}, result.match->begin, options);
  expect_match(result, {{0, 0}, {0, 2}}, "backward previous paragraph");

  options.wrap = true;
  result = find_next(model, JwpText{'a', 'b'}, {0, 0}, options);
  expect_match(result, {{2, 2}, {2, 4}}, "backward wrapped match");
  expect(result.wrapped, "backward wrap is reported");

  JwpDocumentModel only_start = model_with_paragraphs({{'A'}});
  expect(!find_next(only_start, JwpText{'A'}, {0, 0}, options).match,
         "backward wrap never repeats starting candidate");
  JwpDocumentModel end_start = model_with_paragraphs({{'B', 'A'}});
  const auto end_result = find_next(end_start, JwpText{'A'}, {0, 2}, options);
  expect_match(end_result, {{0, 1}, {0, 2}},
               "backward end offset finds preceding match");
  expect(!end_result.wrapped, "backward end offset does not wrap");
}

void test_ascii_case_and_jascii_equivalence() {
  JwpDocumentModel model = model_with_paragraphs(
      {{'x', 0x2341, 0x2362, 0x2331, 0x2127, 0x2422}});

  auto result = find_next(model, JwpText{'a', 'B', '1'}, {0, 0});
  expect_match(result, {{0, 1}, {0, 4}}, "default ASCII/JASCII folding");

  JwpSearchOptions exact;
  exact.ignore_ascii_case = false;
  exact.jascii_ascii_equivalence = false;
  expect(!find_next(model, JwpText{'a', 'B', '1'}, {0, 0}, exact).match,
         "disabled folding is exact");

  JwpSearchOptions case_sensitive;
  case_sensitive.ignore_ascii_case = false;
  expect(!find_next(model, JwpText{'a', 'B', '1'}, {0, 0}, case_sensitive)
              .match,
         "JASCII mapping does not imply case folding");

  expect(!find_next(model, JwpText{':'}, {0, 0}).match,
         "JASCII option does not normalize Japanese punctuation");
  expect_match(find_next(model, JwpText{0x2422}, {0, 4}),
               {{0, 5}, {0, 6}}, "Japanese token remains exact");
}

void test_empty_pattern_and_invalid_start() {
  JwpDocumentModel model = model_with_paragraphs({{'A'}});
  const auto empty = find_next(model, {}, {0, 0});
  expect(empty.empty_pattern && !empty.match && !empty.wrapped,
         "empty pattern is explicit no-op");
  expect_error([&] { find_next(model, JwpText{'A'}, {1, 0}); },
               "invalid search paragraph");
  expect_error([&] { find_next(model, JwpText{'A'}, {0, 2}); },
               "invalid search offset");
}

void test_undefined_jascii_tokens_follow_legacy_comparison() {
  JwpDocumentModel model = model_with_paragraphs({{'X', 0x2321}});
  expect_match(find_next(model, JwpText{0x2322}, {0, 0}),
               {{0, 1}, {0, 2}},
               "undefined JASCII tokens share legacy zero mapping");
  expect_match(find_next(model, JwpText{0}, {0, 0}), {{0, 1}, {0, 2}},
               "undefined JASCII token compares with zero");

  JwpSearchOptions exact;
  exact.jascii_ascii_equivalence = false;
  expect(!find_next(model, JwpText{0x2322}, {0, 0}, exact).match,
         "undefined JASCII tokens differ in exact mode");
}

void test_find_all_is_document_order_and_non_overlapping() {
  JwpDocumentModel model =
      model_with_paragraphs({{'A', 'A', 'A', 'A'}, {}, {'A', 'A'}});
  JwpSearchOptions options;
  options.direction = JwpSearchDirection::kBackward;
  options.wrap = true;
  const std::vector<JwpRange> matches =
      find_all(model, JwpText{'a', 'a'}, options);
  expect(matches.size() == 3, "find all match count");
  expect(matches[0].begin == JwpPosition{0, 0} &&
             matches[0].end == JwpPosition{0, 2},
         "find all includes offset zero");
  expect(matches[1].begin == JwpPosition{0, 2} &&
             matches[1].end == JwpPosition{0, 4},
         "find all returns adjacent non-overlapping match");
  expect(matches[2].begin == JwpPosition{2, 0} &&
             matches[2].end == JwpPosition{2, 2},
         "find all continues in document order");

  expect(find_all(model, JwpText{'A', 'A', 'A'}).size() == 1,
         "find all skips overlapping candidates");
  expect(find_all(model, {}).empty(), "empty find-all pattern is a no-op");
  expect(find_all(model_with_paragraphs({{'A'}, {'A'}}), JwpText{'A', 'A'})
             .empty(),
         "find all does not cross paragraphs");
}

void test_find_all_honors_comparison_options() {
  JwpDocumentModel model = model_with_paragraphs({{0x2341, 'A'}});
  expect(find_all(model, JwpText{'a'}).size() == 2,
         "find all applies default ASCII and JASCII folding");

  JwpSearchOptions case_sensitive;
  case_sensitive.ignore_ascii_case = false;
  expect(find_all(model, JwpText{'a'}, case_sensitive).empty(),
         "find all honors case-sensitive comparison");

  JwpSearchOptions no_jascii;
  no_jascii.jascii_ascii_equivalence = false;
  const std::vector<JwpRange> ascii_only =
      find_all(model, JwpText{'a'}, no_jascii);
  expect(ascii_only.size() == 1 &&
             ascii_only[0].begin == JwpPosition{0, 1},
         "find all honors disabled JASCII equivalence");
}

void test_replace_is_strong_and_composes_with_history() {
  JwpDocumentModel model = model_with_paragraphs({{'A', 'B'}, {'C', 'D'}});
  JwpDocumentHistory history;
  JwpPosition caret{0, 0};

  history.begin(model, caret);
  caret = replace_range(model, {{0, 1}, {1, 1}}, JwpText{'X', 'Y'});
  expect(history.commit(model, caret), "replacement history commit");
  expect(model.paragraph_count() == 1 &&
             model.paragraph(0).text == JwpText({'A', 'X', 'Y', 'D'}),
         "cross-paragraph replacement");
  expect(caret == JwpPosition{0, 3}, "replacement caret follows insertion");
  expect(history.undo(model, caret), "replacement undo");
  expect(model.paragraph_count() == 2 &&
             model.paragraph(0).text == JwpText({'A', 'B'}),
         "replacement undo restores whole document");

  JwpDocument malformed;
  JwpParagraph page_break;
  page_break.page_break = true;
  page_break.text = {'A'};
  malformed.paragraphs.push_back(page_break);
  JwpDocumentModel malformed_model(std::move(malformed));
  const JwpDocument before = malformed_model.document();
  try {
    replace_range(malformed_model, {{0, 0}, {0, 1}}, JwpText{'B'});
    expect(false, "malformed page-break replacement should fail");
  } catch (const jwpqt::core::JwpDocumentEditError&) {
  }
  expect(malformed_model.document() == before,
         "failed replacement leaves document unchanged");
}

void test_each_replacement_can_be_a_separate_history_entry() {
  JwpDocumentModel model = model_with_paragraphs({{'A', 'A', 'A'}});
  JwpDocumentHistory history;
  JwpPosition caret{};

  history.begin(model, caret);
  caret = replace_range(model, {{0, 0}, {0, 1}}, JwpText{'B'});
  expect(history.commit(model, caret), "first replacement commit");
  history.begin(model, caret);
  caret = replace_range(model, {{0, 1}, {0, 2}}, JwpText{'B'});
  expect(history.commit(model, caret), "second replacement commit");
  expect(history.undo_depth() == 2, "replacements remain separate entries");
  expect(history.undo(model, caret) && model.paragraph(0).text ==
                                           JwpText({'B', 'A', 'A'}),
         "second replacement undoes independently");
  expect(history.undo(model, caret) && model.paragraph(0).text ==
                                           JwpText({'A', 'A', 'A'}),
         "first replacement undo preserves original");
}

}  // namespace

int main() {
  test_forward_excludes_start_and_matches_within_paragraphs();
  test_forward_wrap_stops_before_start();
  test_backward_and_backward_wrap();
  test_ascii_case_and_jascii_equivalence();
  test_empty_pattern_and_invalid_start();
  test_undefined_jascii_tokens_follow_legacy_comparison();
  test_find_all_is_document_order_and_non_overlapping();
  test_find_all_honors_comparison_options();
  test_replace_is_strong_and_composes_with_history();
  test_each_replacement_can_be_a_separate_history_entry();
  return 0;
}
