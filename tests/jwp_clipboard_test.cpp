// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_clipboard.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

jwpqt::core::JwpParagraph paragraph(std::initializer_list<jwpqt::core::JisCode> text,
                                    int left, int first, bool page_break = false) {
  jwpqt::core::JwpParagraph result;
  result.text = text;
  result.left_indent = static_cast<std::uint8_t>(left);
  result.first_indent = static_cast<std::int8_t>(first);
  result.page_break = page_break;
  return result;
}

jwpqt::core::JwpDocument document() {
  jwpqt::core::JwpDocument result;
  result.margins.fill(1.0F);
  result.paragraphs = {
      paragraph({'a', 'b', 0x2422, 0x0080}, 4, 2),
      paragraph({0x2522, 'c'}, 6, -2),
      paragraph({'d', 'e', 'f'}, 8, 1),
  };
  return result;
}

void test_copy_and_wire() {
  using namespace jwpqt::core;
  const JwpDocumentModel source(document());
  const JwpDocument copied = copy_jwp_fragment(source, {{0, 1}, {2, 2}});
  require(copied.paragraphs.size() == 3, "copy did not retain paragraph boundaries");
  require(copied.paragraphs[0].text == JwpText({'b', 0x2422, 0x0080}),
          "copy did not retain raw/JIS cells");
  require(copied.paragraphs[0].left_indent == 4 &&
              copied.paragraphs[0].first_indent == 0,
          "copy did not retain format and clear first indentation");
  require(copied.paragraphs[1] == source.paragraph(1),
          "copy changed an intermediate paragraph");
  require(copied.paragraphs[2].text == JwpText({'d', 'e'}),
          "copy did not trim the final paragraph");

  const std::string encoded = encode_jwp_clipboard_fragment(
      {copied, LegacyCodePage::k1253});
  const auto decoded = decode_jwp_clipboard_fragment(encoded);
  require(decoded.document == copied && decoded.code_page == LegacyCodePage::k1253,
          "clipboard wire round trip changed the fragment");

  for (std::size_t offset : {std::size_t{0}, std::size_t{4}, std::size_t{6},
                             std::size_t{8}}) {
    std::string bad = encoded;
    if (offset == 0) bad[0] = 'X';
    else if (offset == 4) bad[4] = 2;
    else if (offset == 6) { bad[6] = 0; bad[7] = 0; }
    else bad[8] = static_cast<char>(bad[8] + 1);
    try {
      (void)decode_jwp_clipboard_fragment(bad);
      throw std::runtime_error("malformed clipboard envelope was accepted");
    } catch (const JwpClipboardError&) {
    }
  }
  try {
    (void)decode_jwp_clipboard_fragment(encoded, encoded.size() - 1);
    throw std::runtime_error("clipboard byte limit was ignored");
  } catch (const JwpClipboardError&) {
  }
}

void test_paste() {
  using namespace jwpqt::core;
  JwpDocument target;
  target.margins.fill(1.0F);
  target.paragraphs = {paragraph({'0', '1', '2', '3'}, 9, 3)};
  JwpDocumentModel model(target);

  JwpDocument one;
  one.margins.fill(1.0F);
  one.paragraphs = {paragraph({0x2422, 0x0081}, 2, -1)};
  const JwpPosition one_caret = paste_jwp_fragment(model, {{0, 1}, {0, 3}}, one);
  require(one_caret == JwpPosition{0, 3}, "single paste returned wrong caret");
  require(model.paragraph(0).text == JwpText({'0', 0x2422, 0x0081, '3'}),
          "single paste did not replace the selection");
  require(model.paragraph(0).left_indent == 9 && model.paragraph(0).first_indent == 3,
          "single paste incorrectly replaced destination format");

  JwpDocument many;
  many.margins.fill(1.0F);
  many.paragraphs = {
      paragraph({'A'}, 1, 0), paragraph({'B'}, 2, -1), paragraph({'C'}, 3, 1)};
  const JwpPosition many_caret = paste_jwp_fragment(model, {{0, 1}, {0, 1}}, many);
  require(many_caret == JwpPosition{2, 1}, "multi paste returned wrong caret");
  require(model.paragraph_count() == 3, "multi paste did not create paragraphs");
  require(model.paragraph(0).text == JwpText({'0', 'A'}) &&
              model.paragraph(0).left_indent == 9,
          "multi paste changed the leading destination paragraph");
  require(model.paragraph(1) == many.paragraphs[1],
          "multi paste changed an intermediate paragraph");
  require(model.paragraph(2).text == JwpText({'C', 0x2422, 0x0081, '3'}) &&
              model.paragraph(2).left_indent == 3 &&
              model.paragraph(2).first_indent == 1,
          "multi paste did not apply the final paragraph format and suffix");
}

void test_failures_preserve_destination() {
  using namespace jwpqt::core;
  JwpDocumentModel model(document());
  const JwpDocument before = model.document();
  JwpDocument empty;
  empty.margins.fill(1.0F);
  try {
    (void)paste_jwp_fragment(model, {{0, 0}, {0, 0}}, empty);
    throw std::runtime_error("empty fragment was accepted");
  } catch (const JwpClipboardError&) {
  }
  require(model.document() == before, "failed paste changed the destination");

  JwpDocument page_break;
  page_break.margins.fill(1.0F);
  page_break.paragraphs = {paragraph({'x'}, 0, 0), paragraph({}, 0, 0, true)};
  try {
    (void)paste_jwp_fragment(model, {{0, 1}, {0, 1}}, page_break);
    throw std::runtime_error("page-break suffix conflict was accepted");
  } catch (const JwpClipboardError&) {
  }
  require(model.document() == before, "page-break paste failure changed destination");

  try {
    (void)copy_jwp_fragment(model, {{2, 0}, {1, 0}});
    throw std::runtime_error("reversed copy range was accepted");
  } catch (const JwpClipboardError&) {
  }
}

}  // namespace

int main() {
  try {
    test_copy_and_wire();
    test_paste();
    test_failures_preserve_destination();
    std::cout << "All JWP clipboard tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
