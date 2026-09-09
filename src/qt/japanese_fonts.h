// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "application_settings.h"
#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/legacy_code_page.h"
#include <QFont>
#include <QTextFormat>

class QWidget;
class QTextDocument;

namespace jwpqt::qt {

void assign_japanese_font(QWidget& widget, JapaneseFontRole role, bool horizontal_strip = false);
QFont japanese_font(const QWidget& widget, JapaneseFontRole role);
QFont ensure_ascii_font(QFont font);
// Display-only identity: 1 is a legacy byte, 2 is JIS. Zero is Unicode text.
inline constexpr int kJwpCharacterKind = QTextFormat::UserProperty + 0x4a01;
QFont jwp_representation_font(QFont font, int kind);
void apply_jwp_character_fonts(QTextDocument& document, const core::JwpDocument& source,
                               core::LegacyCodePage code_page);
struct ClipboardBitmapOptions {
  bool enabled = true;
  bool vertical = false;
  bool colors = false;
};
ClipboardBitmapOptions clipboard_bitmap_options(const QWidget& widget);
QFont japanese_print_font(QFont base, const JapaneseFontSetting& setting, const QString& directory);
QStringList set_japanese_fonts(QWidget& owner, const ApplicationSettings& settings,
                              const QString& directory = {});

}  // namespace jwpqt::qt
