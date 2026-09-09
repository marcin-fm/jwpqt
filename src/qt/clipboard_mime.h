// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "application_settings.h"

#include <optional>

#include <QByteArray>
#include <QString>

class QMimeData;

namespace jwpqt::qt {

inline constexpr char kJwpClipboardMime[] =
    "application/x-jwpqt-jwp-fragment-v1";
inline constexpr char kEncodedClipboardMime[] =
    "application/x-jwpqt-encoded-text-v1";

struct ClipboardText {
  QString text;
  ClipboardTextFormat format = ClipboardTextFormat::kUtf8;
  int code_page = 1252;
};

void add_clipboard_text_formats(QMimeData& mime, const QString& text,
                                ClipboardTextFormat format, int code_page,
                                bool omit_unicode);
std::optional<ClipboardText> read_clipboard_text(
    const QMimeData& mime, ClipboardTextFormat format, int code_page);

}  // namespace jwpqt::qt
