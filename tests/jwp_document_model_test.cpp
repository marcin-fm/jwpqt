#include "jwpqt/core/jwp_document_model.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>

namespace {

using jwpqt::core::JwpDocument;
using jwpqt::core::JwpDocumentEditError;
using jwpqt::core::JwpDocumentModel;
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
  } catch (const JwpDocumentEditError&) {
    return;
  }
  std::cerr << "FAIL: expected JwpDocumentEditError for " << message << '\n';
  std::exit(1);
}

JwpParagraph paragraph(JwpText text, std::int16_t spacing = 100,
                       bool page_break = false) {
  JwpParagraph result;
  result.text = std::move(text);
  result.line_spacing = spacing;
  result.first_indent = -3;
  result.left_indent = 2;
  result.right_indent = 4;
  result.page_break = page_break;
  return result;
}

void test_empty_model_has_one_paragraph() {
  JwpDocumentModel model;
  expect(model.paragraph_count() == 1, "default paragraph count");
  expect(model.paragraph(0).text.empty(), "default paragraph text");

  JwpDocument empty;
  JwpDocumentModel normalized(std::move(empty));
  expect(normalized.paragraph_count() == 1, "empty document normalization");
}

void test_insert_and_erase_within_paragraph() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({'A', 'D'}));
  JwpDocumentModel model(std::move(document));

  const JwpText inserted = {'B', 'C'};
  expect(model.insert({0, 1}, inserted) == JwpPosition{0, 3},
         "insert cursor");
  expect(model.paragraph(0).text == JwpText({'A', 'B', 'C', 'D'}),
         "insert content");
  expect(model.erase({{0, 1}, {0, 3}}) == JwpPosition{0, 1},
         "erase cursor");
  expect(model.paragraph(0).text == JwpText({'A', 'D'}), "erase content");
}

void test_insert_snapshots_aliased_text() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({'A', 'B'}));
  JwpDocumentModel model(std::move(document));

  const JwpText& aliased = model.paragraph(0).text;
  expect(model.insert({0, 2}, aliased) == JwpPosition{0, 4},
         "aliased insert cursor");
  expect(model.paragraph(0).text == JwpText({'A', 'B', 'A', 'B'}),
         "aliased insert content");
}

void test_split_copies_format_and_moves_suffix() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({'A', 'B', 'C'}, 135));
  JwpDocumentModel model(std::move(document));

  expect(model.split_paragraph({0, 1}) == JwpPosition{1, 0},
         "split cursor");
  expect(model.paragraph_count() == 2, "split paragraph count");
  expect(model.paragraph(0).text == JwpText({'A'}), "split prefix");
  expect(model.paragraph(1).text == JwpText({'B', 'C'}), "split suffix");
  const auto& next = model.paragraph(1);
  expect(next.line_spacing == 135 && next.first_indent == -3 &&
             next.left_indent == 2 && next.right_indent == 4 &&
             !next.page_break,
         "split format copy");
}

void test_split_moves_page_break_forward() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({}, 120, true));
  JwpDocumentModel model(std::move(document));

  model.split_paragraph({0, 0});
  expect(model.paragraph_count() == 2, "page-break split count");
  expect(!model.paragraph(0).page_break && model.paragraph(1).page_break,
         "page-break moves to successor");
  expect(model.paragraph(1).line_spacing == 120,
         "page-break format preserved");
}

void test_erase_across_paragraphs() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({'A', 'B'}, 110));
  document.paragraphs.push_back(paragraph({'C', 'D'}, 120));
  document.paragraphs.push_back(paragraph({'E', 'F'}, 130));
  JwpDocumentModel model(std::move(document));

  model.erase({{0, 1}, {2, 1}});
  expect(model.paragraph_count() == 1, "cross-range paragraph count");
  expect(model.paragraph(0).text == JwpText({'A', 'F'}),
         "cross-range merged text");
  expect(model.paragraph(0).line_spacing == 110,
         "cross-range keeps first format");
}

void test_join_matches_legacy_page_break_cases() {
  JwpDocument normal;
  normal.paragraphs.push_back(paragraph({'A'}, 110, true));
  normal.paragraphs.push_back(paragraph({'B'}, 120));
  JwpDocumentModel joined(std::move(normal));
  expect(joined.join_with_next(0) == JwpPosition{0, 1}, "join cursor");
  expect(joined.paragraph(0).text == JwpText({'A', 'B'}) &&
             !joined.paragraph(0).page_break,
         "join clears page break and appends");

  JwpDocument before_break;
  before_break.paragraphs.push_back(paragraph({}, 110));
  before_break.paragraphs.push_back(paragraph({}, 120, true));
  JwpDocumentModel removed(std::move(before_break));
  removed.join_with_next(0);
  expect(removed.paragraph_count() == 1 && removed.paragraph(0).page_break &&
             removed.paragraph(0).line_spacing == 120,
         "empty predecessor removal preserves page break");
}

void test_set_page_break_clears_text() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({'A', 'B'}, 145));
  JwpDocumentModel model(std::move(document));

  model.set_page_break(0, true);
  expect(model.paragraph(0).page_break && model.paragraph(0).text.empty(),
         "setting page break clears text");
  expect(model.paragraph(0).line_spacing == 145,
         "setting page break preserves format");
  model.set_page_break(0, false);
  expect(!model.paragraph(0).page_break && model.paragraph(0).text.empty(),
         "clearing page break keeps paragraph empty");
}

void test_malformed_page_break_split_is_rejected() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({'A'}, 100, true));
  JwpDocumentModel model(std::move(document));
  expect_error([&] { model.split_paragraph({0, 0}); },
               "page break containing hidden text");
  expect(model.paragraph_count() == 1 && model.paragraph(0).text == JwpText{'A'} &&
             model.paragraph(0).page_break,
         "malformed page-break data is preserved");
}

void test_invalid_edits_leave_document_unchanged() {
  JwpDocument document;
  document.paragraphs.push_back(paragraph({'A'}));
  document.paragraphs.push_back(paragraph({}, 100, true));
  JwpDocumentModel model(std::move(document));
  const JwpDocument before = model.document();
  const JwpText text = {'B'};

  expect_error([&] { model.insert({2, 0}, text); }, "invalid paragraph");
  expect_error([&] { model.insert({0, 2}, text); }, "invalid offset");
  expect_error([&] { model.insert({1, 0}, text); }, "page-break insertion");
  expect_error([&] { model.erase({{0, 1}, {0, 0}}); }, "reversed range");
  expect_error([&] { model.erase({{0, 0}, {0, 2}}); },
               "invalid erase offset");
  expect_error([&] { model.erase({{0, 0}, {2, 0}}); },
               "invalid erase paragraph");
  expect_error([&] { model.split_paragraph({0, 2}); }, "invalid split");
  expect_error([&] { model.join_with_next(1); }, "join at end");
  expect_error(
      [&] { model.join_with_next(std::numeric_limits<std::size_t>::max()); },
      "overflowing join index");
  expect_error([&] { model.set_page_break(2, true); }, "invalid page break");
  expect_error([&] { model.paragraph(2); }, "paragraph access");
  expect(model.document().paragraphs == before.paragraphs,
         "failed edits preserve paragraphs");
}

}  // namespace

int main() {
  test_empty_model_has_one_paragraph();
  test_insert_and_erase_within_paragraph();
  test_insert_snapshots_aliased_text();
  test_split_copies_format_and_moves_suffix();
  test_split_moves_page_break_forward();
  test_erase_across_paragraphs();
  test_join_matches_legacy_page_break_cases();
  test_set_page_break_clears_text();
  test_malformed_page_break_split_is_rejected();
  test_invalid_edits_leave_document_unchanged();
  return 0;
}
