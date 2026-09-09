// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "jwpqt/core/jwp_document_model.h"
#include "jwpqt/core/legacy_code_page.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

struct JwpClipboardFragment {
  JwpDocument document;
  LegacyCodePage code_page = kDefaultLegacyCodePage;
};

class JwpClipboardError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

JwpDocument copy_jwp_fragment(const JwpDocumentModel& source, JwpRange range);
JwpPosition paste_jwp_fragment(JwpDocumentModel& destination, JwpRange range,
                               const JwpDocument& fragment);

std::string encode_jwp_clipboard_fragment(
    const JwpClipboardFragment& fragment,
    std::size_t maximum_bytes = 64U * 1024U * 1024U);
JwpClipboardFragment decode_jwp_clipboard_fragment(
    std::string_view bytes,
    std::size_t maximum_bytes = 64U * 1024U * 1024U);

}  // namespace jwpqt::core
