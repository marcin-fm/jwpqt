// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_FILE_IO_H
#define JWPQT_QT_FILE_IO_H

#include <cstddef>
#include <optional>
#include <string>

#include <QString>

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/text_file.h"
#include "jwpqt/core/wnn_preferences.h"

namespace jwpqt::qt {

std::string read_file_bytes(const QString& path);
core::TextFile read_text_file(const QString& path,
                              core::TextEncoding encoding);
void write_text_file(const QString& path, const core::TextFile& file);
core::JwpDocument read_jwp_file(const QString& path);
void write_jwp_file(const QString& path, const core::JwpDocument& document);
std::optional<core::WnnPreferences> read_wnn_preferences_file(
    const QString& path,
    std::size_t capacity = core::kWnnDefaultPreferenceCapacity);
void write_wnn_preferences_file(const QString& path,
                                core::WnnPreferences& preferences);

}  // namespace jwpqt::qt

#endif
