// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "jwpqt/core/jwp_plain_text.h"

namespace {

using jwpqt::core::JwpDocument;
using jwpqt::core::JwpDocumentModel;
using jwpqt::core::JwpParagraph;
using jwpqt::core::JwpPlainTextError;
using jwpqt::core::JwpPosition;
using jwpqt::core::JwpText;
using jwpqt::core::LegacyCodePage;
using jwpqt::core::decode_jwp_plain_text;
using jwpqt::core::jwp_plain_text_offset;
using jwpqt::core::jwp_plain_text_position;
using jwpqt::core::jwp_plain_text_size;
using jwpqt::core::replace_jwp_plain_text;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Function>
void expect_edit_error(Function&& function, std::string_view message) {
  bool threw = false;
  try {
    std::forward<Function>(function)();
  } catch (const JwpPlainTextError&) {
    threw = true;
  }
  expect(threw, message);
}

JwpDocumentModel model_with_paragraphs(std::vector<JwpParagraph> paragraphs) {
  JwpDocument document;
  document.paragraphs = std::move(paragraphs);
  return JwpDocumentModel(std::move(document));
}

JwpParagraph paragraph(JwpText text, std::int8_t first_indent = 0,
                       bool page_break = false) {
  JwpParagraph value;
  value.text = std::move(text);
  value.first_indent = first_indent;
  value.page_break = page_break;
  return value;
}

void test_flatten_and_offset_mapping() {
  JwpDocumentModel model = model_with_paragraphs(
      {paragraph({'A', 'B'}), paragraph({}), paragraph({'C'})});
  expect(jwp_plain_text_size(model) == 5, "plain-text size is wrong");
  expect(decode_jwp_plain_text(model) == U"AB\n\nC",
         "paragraphs did not flatten with newlines");

  const std::vector<JwpPosition> positions{{0, 0}, {0, 1}, {0, 2},
                                            {1, 0}, {2, 0}, {2, 1}};
  const std::vector<std::size_t> offsets{0, 1, 2, 3, 4, 5};
  for (std::size_t index = 0; index < positions.size(); ++index) {
    expect(jwp_plain_text_position(model, offsets[index]) == positions[index],
           "flat offset mapped to the wrong JWP position");
    expect(jwp_plain_text_offset(model, positions[index]) == offsets[index],
           "JWP position mapped to the wrong flat offset");
  }

  const std::vector<std::vector<JwpParagraph>> empty_cases{
      {paragraph({})},
      {paragraph({'A'}), paragraph({})},
      {paragraph({}), paragraph({'A'})},
      {paragraph({}), paragraph({}), paragraph({})},
  };
  for (const std::vector<JwpParagraph>& paragraphs : empty_cases) {
    JwpDocumentModel empty_model = model_with_paragraphs(paragraphs);
    for (std::size_t offset = 0; offset <= jwp_plain_text_size(empty_model);
         ++offset) {
      const JwpPosition position =
          jwp_plain_text_position(empty_model, offset);
      expect(jwp_plain_text_offset(empty_model, position) == offset,
             "empty-paragraph offset did not round-trip");
    }
  }
}

void test_insert_and_delete_across_paragraphs() {
  JwpDocumentModel model =
      model_with_paragraphs({paragraph({'A', 'B'}, 3), paragraph({'C'}, 7)});
  const JwpPosition inserted = replace_jwp_plain_text(model, 1, 0, U"x\ny");
  expect(inserted == JwpPosition{1, 1}, "insert returned wrong caret");
  expect(decode_jwp_plain_text(model) == U"Ax\nyB\nC",
         "multiline insertion produced wrong text");
  expect(model.paragraph(0).first_indent == 3 &&
             model.paragraph(1).first_indent == 3 &&
             model.paragraph(2).first_indent == 7,
         "multiline insertion did not preserve split formatting");

  const JwpPosition erased = replace_jwp_plain_text(model, 1, 4, U"");
  expect(erased == JwpPosition{0, 1}, "erase returned wrong caret");
  expect(decode_jwp_plain_text(model) == U"A\nC",
         "cross-paragraph erase produced wrong text");
  expect(model.paragraph(0).first_indent == 3,
         "cross-paragraph erase lost first paragraph formatting");
}

void test_code_page_and_page_break_behavior() {
  JwpDocumentModel model = model_with_paragraphs({paragraph({0x80})});
  expect(decode_jwp_plain_text(model, LegacyCodePage::k1251) == U"\u0402",
         "selected code page was not used while decoding");
  replace_jwp_plain_text(model, 1, 0, U"\u0403", LegacyCodePage::k1251);
  expect(model.paragraph(0).text == JwpText({0x80, 0x81}),
         "selected code page was not used while encoding");

  JwpDocumentModel page = model_with_paragraphs(
      {paragraph({'A'}), paragraph({}, 4, true), paragraph({'B'})});
  replace_jwp_plain_text(page, 0, 1, U"C");
  expect(page.paragraph(1).page_break,
         "unrelated edit did not preserve hard page break");
  const JwpDocument before = page.document();
  expect_edit_error([&] { replace_jwp_plain_text(page, 2, 0, U"x"); },
                    "insertion into hard page break was accepted");
  expect(page.document() == before,
         "failed hard-page-break edit changed document");
  expect_edit_error([&] { replace_jwp_plain_text(page, 1, 1, U""); },
                    "deletion before hard page break was accepted");
  expect_edit_error([&] { replace_jwp_plain_text(page, 2, 1, U""); },
                    "deletion after hard page break was accepted");
  expect(page.document() == before,
         "failed hard-page-break deletion changed document");
}

void test_failure_is_strong_and_bounds_are_checked() {
  JwpDocumentModel model = model_with_paragraphs({paragraph({'A', 'B'})});
  const JwpDocument before = model.document();
  expect_edit_error([&] { replace_jwp_plain_text(model, 1, 1, U"\U0001F600"); },
                    "unrepresentable replacement was accepted");
  expect(model.document() == before,
         "failed encoding changed the original document");

  expect_edit_error([&] { jwp_plain_text_position(model, 3); },
                    "past-end flat offset was accepted");
  expect_edit_error([&] { jwp_plain_text_offset(model, {1, 0}); },
                    "invalid JWP position was accepted");
  expect_edit_error(
      [&] {
        replace_jwp_plain_text(model, 1,
                               std::numeric_limits<std::size_t>::max(), U"");
      },
      "overflowing removal length was accepted");
}

void test_embedded_newline_is_rejected() {
  JwpDocumentModel model = model_with_paragraphs({paragraph({'A', '\n', 'B'})});
  expect_edit_error([&] { decode_jwp_plain_text(model); },
                    "embedded paragraph newline was accepted");
  expect_edit_error([&] { jwp_plain_text_size(model); },
                    "size accepted embedded paragraph newline");
  expect_edit_error([&] { jwp_plain_text_position(model, 0); },
                    "position accepted embedded paragraph newline");
  expect_edit_error([&] { jwp_plain_text_offset(model, {0, 0}); },
                    "offset accepted embedded paragraph newline");
  expect_edit_error([&] { replace_jwp_plain_text(model, 0, 0, U"x"); },
                    "edit accepted embedded paragraph newline");
}

}  // namespace

int main() {
  try {
    test_flatten_and_offset_mapping();
    test_insert_and_delete_across_paragraphs();
    test_code_page_and_page_break_behavior();
    test_failure_is_strong_and_bounds_are_checked();
    test_embedded_newline_is_rejected();
    std::cout << "All JWP plain-text tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
