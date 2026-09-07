// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <QString>
#include <QStringList>

#include "kanji_info_options.h"
#include "edict_lookup_options.h"

namespace jwpqt::qt {

enum class JapaneseFontRole { kSystem, kEdit, kList, kKanjiBar, kFile, kBig, kTable, kCount };

struct JapaneseFontSetting {
  QString family;
  int size = 16;
  bool automatic = true;
};

struct ApplicationSettings {
  ApplicationSettings();

  std::array<JapaneseFontSetting, static_cast<std::size_t>(JapaneseFontRole::kCount)> fonts;
  bool show_toolbar = true;
  bool show_status_bar = true;
  bool show_kanji_bar = true;
  bool kanji_bar_at_top = false;
  bool vertical_scrollbar = true;
  bool horizontal_scrollbar = true;
  bool kanji_bar_scrollbar = true;
  bool save_settings_on_exit = true;
  bool save_recent_files = true;
  int translation_code_page = 1252;
  KanjiInfoOptions kanji_info;
  EdictLookupOptions dictionary;
  std::uint32_t dictionary_extra_exclusions = 0;
  std::string source;
  QStringList unapplied;
};

ApplicationSettings read_application_settings(std::string_view text,
                                               const ApplicationSettings& base = {});
std::string write_application_settings(const ApplicationSettings& settings);
ApplicationSettings read_application_settings_file(
    const QString& path, const ApplicationSettings& base = {});
void write_application_settings_file(const QString& path,
                                     const ApplicationSettings& settings);

}  // namespace jwpqt::qt
