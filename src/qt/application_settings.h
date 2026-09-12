// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "toolbar_settings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <QString>
#include <QStringList>

#include "kanji_info_options.h"
#include "edict_lookup_options.h"
#include "jwpqt/core/print_format.h"
#include "jwpqt/core/kanji_color.h"

namespace jwpqt::qt {

enum class JapaneseFontRole { kSystem, kEdit, kList, kKanjiBar, kFile, kBig, kTable, kBitmap, kCount };
enum class DuplicateOpenBehavior { kOpenAnother = 0, kActivateExisting = 1, kPrompt = 2 };
enum class LineWidthMode { kDynamic = 0, kFixed = 1, kPrinter = 2 };
enum class ColorSchemePreference { kSystem = 0, kLight = 1, kDark = 2 };
enum class ClipboardTextFormat {
  kAutoDetect = 1,
  kEucJp = 6,
  kShiftJis = 7,
  kNewJis = 8,
  kOldJis = 9,
  kNecJis = 10,
  kUnicode = 11,
  kUtf7 = 12,
  kUtf8 = 13,
};

struct JapaneseFontSetting {
  QString family;
  int size = 16;
  bool automatic = true;
};

struct ApplicationSettings {
  ApplicationSettings();

  std::array<JapaneseFontSetting, static_cast<std::size_t>(JapaneseFontRole::kCount)> fonts;
  JapaneseFontSetting print_font{{}, 120, true}; // Size is tenths of a point, not screen pixels.
  JapaneseFontSetting ascii_font{{}, 0, true}; // Source size/automatic are retained; height follows each Japanese role.
  bool show_toolbar = true;
  ColorSchemePreference color_scheme = ColorSchemePreference::kSystem;
  bool omit_clipboard_bitmap = true;
  ClipboardTextFormat clipboard_export = ClipboardTextFormat::kShiftJis;
  ClipboardTextFormat clipboard_import = ClipboardTextFormat::kUnicode;
  bool omit_clipboard_unicode = false;
  bool vertical_clipboard_bitmap = false;
  bool color_clipboard_bitmap = false;
  bool color_printing = false;
  core::JwpPrintFormatting print_formatting;
  core::JwpPageDefaults default_page;
  bool metric_units = false;
  LineWidthMode line_width_mode = LineWidthMode::kDynamic;
  int fixed_line_width = 35;
  bool relax_margin_punctuation = true;
  bool relax_margin_small_kana = true;
  // Absent legacy overrides leave the existing native color store authoritative.
  std::array<std::optional<std::uint32_t>, 3> color_refs; // Highlight, list, uncommon (COLORREF).
  std::optional<int> color_kanji_mode;
  std::optional<bool> colorize_rare;
  bool mark_rare_kanji = false;
  ToolbarSettings toolbar;
  bool show_status_bar = true;
  bool show_kanji_bar = true;
  bool kanji_bar_at_top = false;
  bool vertical_scrollbar = true;
  bool horizontal_scrollbar = true;
  bool kanji_bar_scrollbar = true;
  bool save_settings_on_exit = true;
  bool save_recent_files = true;
  bool save_histories = true;
  bool revert_to_kanji_mode = true;
  bool old_katakana_input = false;
  bool ctrl_up_down_convert = false;
  bool insert_on_separate_lines = true;
  bool show_all_fonts = false;
  bool auto_scroll = true;
  int auto_scroll_speed = 100;
  DuplicateOpenBehavior duplicate_open = DuplicateOpenBehavior::kPrompt;
  bool startup_dictionary = false;
  bool reload_previous_files = false;
  bool keep_backup_copy = false;
  bool restore_window = false;
  bool maximize_window = false;
  // Main, Character Info, Dictionary, Count, More Info, User Conversion, User Dictionary.
  // Each tuple is x/y/width/height in native logical pixels; zero dimensions mean defaults.
  std::array<std::array<int, 4>, 7> window_geometry{};
  bool close_button_closes_file = false;
  bool confirm_last_file_exit = true;
  bool automatic_kanji_lookup = true;
  bool rare_kanji_last = true;
  bool bushu_nelson = true;
  bool bushu_classical = true;
  bool flexible_kun = true;
  bool partial_meanings = false;
  bool skip_miscodes = false;
  bool reduce_radical_choices = false;
  bool deemphasize_rare_radicals = true;
  int index_type = 0;
  int reading_type = 2;
  bool search_all_files = false;
  bool search_ignore_case = true;
  bool search_ignore_width = true;
  bool search_wrap = false;
  bool search_keep_open = true;
  int conversion_choices = 200;
  int history_size = 300;
  int maximum_undo_levels = 50;
  int translation_code_page = 0;
  KanjiInfoOptions kanji_info;
  EdictLookupOptions dictionary;
  std::uint32_t dictionary_extra_exclusions = 0;
  std::string source;
  QStringList unapplied;
};

ApplicationSettings read_application_settings(std::string_view text,
                                               const ApplicationSettings& base = {});
core::KanjiColorPolicy effective_kanji_color_policy(
    const ApplicationSettings& settings, core::KanjiColorPolicy base);
std::string write_application_settings(const ApplicationSettings& settings);
ApplicationSettings read_application_settings_file(
    const QString& path, const ApplicationSettings& base = {});
void write_application_settings_file(const QString& path,
                                     const ApplicationSettings& settings);

}  // namespace jwpqt::qt
