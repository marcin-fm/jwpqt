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

void test_loss_aware_text_transfer() {
  using jwpqt::core::import_jwp_plain_text;
  using jwpqt::core::export_jwp_plain_text;
  for (const auto text : {U"", U"one\n two\n", U"\n\n", U"\u65e5\u672c\tA"}) {
    const auto model = import_jwp_plain_text(text);
    const auto before = model.document();
    const auto result = export_jwp_plain_text(model);
    expect(result.text == text && result.lossless(),
           "Plain import/export changed text or reported spurious losses");
    expect(model.document() == before, "Text export modified the source model");
  }
  const auto cyrillic = import_jwp_plain_text(U"\u0402\u0403", LegacyCodePage::k1251);
  expect(cyrillic.paragraph(0).text == JwpText({0x80, 0x81}) &&
             export_jwp_plain_text(cyrillic, LegacyCodePage::k1251).text == U"\u0402\u0403",
         "Text transfer ignored the selected code page");
  for (const std::u32string& text : {
           std::u32string(U"\r\n"), std::u32string(U"\u2028"),
           std::u32string(U"\u2029"), std::u32string(U"\U0001f600"),
           std::u32string{U'A', U'\0'}, std::u32string{0xd800},
           std::u32string{0x110000}}) {
    expect_edit_error([&] { import_jwp_plain_text(text); },
                      "Unrepresentable or unnormalized import was accepted");
  }
  auto document = import_jwp_plain_text(U"source").document();
  document.paragraphs[0].line_spacing = 120;
  auto result = export_jwp_plain_text(JwpDocumentModel(document));
  expect(result.loses_formatting && !result.loses_metadata &&
             !result.loses_page_breaks && !result.lossless(),
         "Paragraph formatting loss was not isolated");
  document.paragraphs[0].line_spacing = 100;
  document.margins[0] = 0.5F;
  expect(export_jwp_plain_text(JwpDocumentModel(document)).loses_formatting,
         "Page layout loss was not reported");
  document.margins[0] = 1.0F;
  document.summary[0] = {'A'};
  result = export_jwp_plain_text(JwpDocumentModel(document));
  expect(result.loses_metadata && !result.loses_formatting &&
             !result.loses_page_breaks, "Summary loss was not isolated");
  document.summary[0].clear();
  document.headers[3][2] = {'B'};
  expect(export_jwp_plain_text(JwpDocumentModel(document)).loses_metadata,
         "Header text loss was not reported");
  document.headers[3][2].clear();
  document.paragraphs.emplace_back();
  document.paragraphs.back().page_break = true;
  result = export_jwp_plain_text(JwpDocumentModel(document));
  expect(result.text == U"source\n" && result.loses_page_breaks &&
             !result.loses_metadata && !result.loses_formatting,
         "Hard-page-break loss was not isolated");
}

}  // namespace

int main() {
  try {
    test_flatten_and_offset_mapping();
    test_insert_and_delete_across_paragraphs();
    test_code_page_and_page_break_behavior();
    test_failure_is_strong_and_bounds_are_checked();
    test_embedded_newline_is_rejected();
    test_loss_aware_text_transfer();
    std::cout << "All JWP plain-text tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
