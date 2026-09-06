// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_JWP_PLAIN_TEXT_H
#define JWPQT_CORE_JWP_PLAIN_TEXT_H

#include <cstddef>
#include <string>
#include <string_view>

#include "jwpqt/core/jwp_document_model.h"
#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::core {

class JwpPlainTextError : public JwpDocumentEditError {
 public:
  using JwpDocumentEditError::JwpDocumentEditError;
};

struct JwpPlainTextExport {
  std::u32string text;
  bool loses_formatting = false;
  bool loses_metadata = false;
  bool loses_page_breaks = false;

  bool lossless() const noexcept {
    return !loses_formatting && !loses_metadata && !loses_page_breaks;
  }
};

// Import expects LF-normalized text and never replaces unmappable characters.
JwpDocumentModel import_jwp_plain_text(
    std::u32string_view text,
    LegacyCodePage code_page = kDefaultLegacyCodePage);
JwpPlainTextExport export_jwp_plain_text(
    const JwpDocumentModel& model,
    LegacyCodePage code_page = kDefaultLegacyCodePage);

std::size_t jwp_plain_text_size(const JwpDocumentModel& model);
JwpPosition jwp_plain_text_position(const JwpDocumentModel& model,
                                    std::size_t offset);
std::size_t jwp_plain_text_offset(const JwpDocumentModel& model,
                                  JwpPosition position);

std::u32string decode_jwp_plain_text(
    const JwpDocumentModel& model,
    LegacyCodePage code_page = kDefaultLegacyCodePage);

JwpPosition replace_jwp_plain_text(
    JwpDocumentModel& model, std::size_t offset, std::size_t removed_length,
    std::u32string_view replacement,
    LegacyCodePage code_page = kDefaultLegacyCodePage);

}  // namespace jwpqt::core

#endif
