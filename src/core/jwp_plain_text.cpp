// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_plain_text.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "jwpqt/core/jwp_text_codec.h"

namespace jwpqt::core {
namespace {

std::size_t checked_plain_text_size(const JwpDocumentModel& model) {
  std::size_t size = model.paragraph_count() - 1;
  for (std::size_t index = 0; index < model.paragraph_count(); ++index) {
    const JwpParagraph& paragraph = model.paragraph(index);
    if (std::find(paragraph.text.begin(), paragraph.text.end(), '\n') !=
        paragraph.text.end()) {
      throw JwpPlainTextError("JWP paragraph contains an embedded newline");
    }
    if (paragraph.page_break && !paragraph.text.empty()) {
      throw JwpPlainTextError("JWP hard page break contains text");
    }
    const std::size_t paragraph_size = paragraph.text.size();
    if (paragraph_size > std::numeric_limits<std::size_t>::max() - size) {
      throw JwpPlainTextError("JWP document is too large");
    }
    size += paragraph_size;
  }
  return size;
}

bool deletion_touches_page_break(const JwpDocumentModel& model,
                                 JwpPosition begin, JwpPosition end) {
  if (begin.paragraph == end.paragraph) {
    return false;
  }
  for (std::size_t paragraph = begin.paragraph;; ++paragraph) {
    if (model.paragraph(paragraph).page_break) {
      return true;
    }
    if (paragraph == end.paragraph) {
      return false;
    }
  }
}

JwpPosition insert_plain_text(JwpDocumentModel& model, JwpPosition position,
                              std::u32string_view replacement,
                              LegacyCodePage code_page) {
  std::size_t begin = 0;
  while (begin <= replacement.size()) {
    const std::size_t newline = replacement.find(U'\n', begin);
    const std::size_t end = newline == std::u32string_view::npos
                                ? replacement.size()
                                : newline;
    if (end != begin) {
      position = model.insert(
          position, encode_jwp_text(replacement.substr(begin, end - begin),
                                    code_page));
    }
    if (newline == std::u32string_view::npos) {
      break;
    }
    position = model.split_paragraph(position);
    begin = newline + 1;
  }
  return position;
}

}  // namespace

JwpDocumentModel import_jwp_plain_text(std::u32string_view text,
                                      LegacyCodePage code_page) {
  JwpDocument document;
  document.margins.fill(1.0F);
  std::size_t begin = 0;
  while (true) {
    const std::size_t newline = text.find(U'\n', begin);
    const auto part = text.substr(begin, newline == std::u32string_view::npos
                                           ? text.size() - begin
                                           : newline - begin);
    if (part.find_first_of(U"\r\u2028\u2029") != std::u32string_view::npos) {
      throw JwpPlainTextError("JWP text import requires LF-normalized text");
    }
    JwpParagraph paragraph;
    try {
      paragraph.text = encode_jwp_text(part, code_page);
      if (decode_jwp_text(paragraph.text, code_page) != part) {
        throw JwpPlainTextError("JWP text import would change Unicode characters");
      }
    } catch (const JwpTextCodecError& error) {
      throw JwpPlainTextError("JWP text import at paragraph offset " +
                             std::to_string(begin) + ": " + error.what());
    }
    document.paragraphs.push_back(std::move(paragraph));
    if (newline == std::u32string_view::npos) break;
    begin = newline + 1;
  }
  return JwpDocumentModel(std::move(document));
}

JwpPlainTextExport export_jwp_plain_text(const JwpDocumentModel& model,
                                        LegacyCodePage code_page) {
  JwpPlainTextExport result;
  result.text = decode_jwp_plain_text(model, code_page);
  const JwpDocument& document = model.document();
  result.loses_formatting = document.landscape || document.vertical ||
      document.separate_left_right_headers || document.suppress_first_page_headers ||
      document.margins != std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F};
  for (const JwpParagraph& paragraph : document.paragraphs) {
    result.loses_formatting |= paragraph.line_spacing != 100 ||
        paragraph.first_indent != 0 || paragraph.left_indent != 0 ||
        paragraph.right_indent != 0;
    result.loses_page_breaks |= paragraph.page_break;
  }
  for (const JwpText& field : document.summary) {
    result.loses_metadata |= !field.empty();
  }
  for (const auto& header : document.headers) {
    for (const JwpText& field : header) {
      result.loses_metadata |= !field.empty();
    }
  }
  return result;
}

std::size_t jwp_plain_text_size(const JwpDocumentModel& model) {
  return checked_plain_text_size(model);
}

JwpPosition jwp_plain_text_position(const JwpDocumentModel& model,
                                    std::size_t offset) {
  const std::size_t size = checked_plain_text_size(model);
  if (offset > size) {
    throw JwpPlainTextError("plain-text offset is out of range");
  }

  for (std::size_t paragraph = 0; paragraph < model.paragraph_count();
       ++paragraph) {
    const std::size_t paragraph_size = model.paragraph(paragraph).text.size();
    if (offset <= paragraph_size) {
      return {paragraph, offset};
    }
    offset -= paragraph_size + 1;
  }
  throw JwpPlainTextError("plain-text offset is out of range");
}

std::size_t jwp_plain_text_offset(const JwpDocumentModel& model,
                                  JwpPosition position) {
  checked_plain_text_size(model);
  if (!model.valid_position(position)) {
    throw JwpPlainTextError("JWP position is out of range");
  }

  std::size_t offset = position.offset;
  for (std::size_t paragraph = 0; paragraph < position.paragraph; ++paragraph) {
    const std::size_t paragraph_size = model.paragraph(paragraph).text.size();
    if (paragraph_size >= std::numeric_limits<std::size_t>::max() - offset) {
      throw JwpPlainTextError("JWP document is too large");
    }
    offset += paragraph_size + 1;
  }
  return offset;
}

std::u32string decode_jwp_plain_text(const JwpDocumentModel& model,
                                     LegacyCodePage code_page) {
  try {
    std::u32string text;
    text.reserve(checked_plain_text_size(model));
    for (std::size_t paragraph = 0; paragraph < model.paragraph_count();
         ++paragraph) {
      const std::u32string decoded =
          decode_jwp_text(model.paragraph(paragraph).text, code_page);
      if (paragraph != 0) {
        text.push_back(U'\n');
      }
      text.append(decoded);
    }
    return text;
  } catch (const JwpPlainTextError&) {
    throw;
  } catch (const JwpTextCodecError& error) {
    throw JwpPlainTextError(error.what());
  }
}

JwpPosition replace_jwp_plain_text(JwpDocumentModel& model,
                                   std::size_t offset,
                                   std::size_t removed_length,
                                   std::u32string_view replacement,
                                   LegacyCodePage code_page) {
  const std::size_t size = checked_plain_text_size(model);
  if (offset > size || removed_length > size - offset) {
    throw JwpPlainTextError("plain-text edit is out of range");
  }

  try {
    JwpDocumentModel edited = model;
    const JwpPosition begin = jwp_plain_text_position(edited, offset);
    const JwpPosition end =
        jwp_plain_text_position(edited, offset + removed_length);
    if (deletion_touches_page_break(edited, begin, end)) {
      throw JwpPlainTextError(
          "plain-text deletion cannot remove a hard page break");
    }
    JwpPosition position = edited.erase({begin, end});
    position = insert_plain_text(edited, position, replacement, code_page);
    model = std::move(edited);
    return position;
  } catch (const JwpPlainTextError&) {
    throw;
  } catch (const JwpDocumentEditError& error) {
    throw JwpPlainTextError(error.what());
  } catch (const JwpTextCodecError& error) {
    throw JwpPlainTextError(error.what());
  }
}

}  // namespace jwpqt::core
