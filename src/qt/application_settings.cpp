// SPDX-License-Identifier: GPL-2.0-or-later

#include "application_settings.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QFile>
#include <QSaveFile>

#include "jwpqt/core/jwp_configuration.h"
#include "jwpqt/core/edict_filter.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

struct FontDescriptor {
  const char* name;
  const char* alias;
};

constexpr FontDescriptor kFonts[] = {{"System", "sys_font"}, {"Edit", "edit_font"},
    {"List", "list_font"}, {"KanjiBar", "bar_font"}, {"File", "file_font"},
    {"Big", "big_font"}, {"Table", "jis_font"}, {"Bitmap", "clip_font"}, {"Print", "print_font"}, {"ASCII", "ascii_font"}};
constexpr FontDescriptor kFontFields[] = {{"Font", "name"}, {"Size", "size"},
    {"Auto", "automatic"}};
static_assert(std::size(kFonts) == static_cast<std::size_t>(JapaneseFontRole::kCount) + 2);

template <typename Owner>
struct BooleanDescriptor {
  const char* name;
  const char* alias;
  bool Owner::*member;
};

constexpr BooleanDescriptor<ApplicationSettings> kBooleans[] = {
    {"ColorKanji_Printing", "colorkanji_print", &ApplicationSettings::color_printing},
    {"Clipboard_Omit_Bitmap", "no_BITMAP", &ApplicationSettings::omit_clipboard_bitmap},
    {"Bitmap.Vert", "clip_font.vertical", &ApplicationSettings::vertical_clipboard_bitmap},
    {"ColorKanji_Clipboard", "colorkanji_bitmap", &ApplicationSettings::color_clipboard_bitmap},
    {"AutoSearch_KanjiLookup", "auto_lookup", &ApplicationSettings::automatic_kanji_lookup},
    {"RareKanjiLast", "rare_last", &ApplicationSettings::rare_kanji_last},
    {"Search_AllFiles", "search_all", &ApplicationSettings::search_all_files},
    {"Search_CaseInsensitive", "search_nocase", &ApplicationSettings::search_ignore_case},
    {"Search_WidthInsensitive", "search_jascii", &ApplicationSettings::search_ignore_width},
    {"Search_WrapAround", "search_wrap", &ApplicationSettings::search_wrap},
    {"Search_KeepDialogsOpen", "keep_find", &ApplicationSettings::search_keep_open},
    {"Show_Toolbar", "toolbar", &ApplicationSettings::show_toolbar},
    {"Show_StatusBar", "status", &ApplicationSettings::show_status_bar},
    {"Show_KanjiBar", "kanjibar", &ApplicationSettings::show_kanji_bar},
    {"KanjiBarAtTop", "kanjibar_top", &ApplicationSettings::kanji_bar_at_top},
    {"ScrollBar_Vertical", "vscroll", &ApplicationSettings::vertical_scrollbar},
    {"ScrollBar_Horizontal", "hscroll", &ApplicationSettings::horizontal_scrollbar},
    {"ScrollBar_KanjiBar", "kscroll", &ApplicationSettings::kanji_bar_scrollbar},
    {"SaveSettingsOnExit", "save_exit", &ApplicationSettings::save_settings_on_exit},
    {"Save_RecentFiles", "save_recent", &ApplicationSettings::save_recent_files},
    {"Save_Histories", "save_history", &ApplicationSettings::save_histories}};

constexpr BooleanDescriptor<EdictLookupOptions> kDictionaryBooleans[] = {
    {"Dict_AutoSearch", "dict_auto", &EdictLookupOptions::automatic_search},
    {"Dict_Compact", "dict_compress", &EdictLookupOptions::compact},
    {"Dict_PriorityEntriesFirst", "dict_primaryfirst", &EdictLookupOptions::priority_first},
    {"Dict_PrioritySeparator", "dict_primark", &EdictLookupOptions::priority_separator},
    {"Dict_Adv_SeparatorMark", "dict_advmark", &EdictLookupOptions::advanced_separator},
    {"Dict_Link_Adv_NoNames", "dict_link_adv_noname", &EdictLookupOptions::link_advanced_names},
    {"Dict_AdvancedSearches", "dict_advanced", &EdictLookupOptions::advanced},
    {"Dict_ContingentSearches", "dict_contingent", &EdictLookupOptions::contingent},
    {"MonitorClipboard", "dict_watchclip", &EdictLookupOptions::monitor_clipboard},
    {"Dict_AlwaysAdvancedSearch", "dict_always", &EdictLookupOptions::advanced_always},
    {"Dict_Adv_KeepSearching", "dict_showall", &EdictLookupOptions::advanced_show_all},
    {"Dict_Adv_Try_I_Adjectives", "dict_iadj", &EdictLookupOptions::i_adjectives},
    {"Dict_ClassicalSupport", "dict_classical", &EdictLookupOptions::classical},
    {"Dict_JASCII_to_ASCII", "dict_jascii2ascii", &EdictLookupOptions::jascii_to_ascii},
    {"Dict_ASCII_MatchFullEntry", "dict_fullascii", &EdictLookupOptions::full_ascii}};

constexpr FontDescriptor kPrintPatterns[] = {{"Printing_Formatting_Date", "date_format"},
    {"Printing_Formatting_Time", "time_format"}, {"Printing_Formatting_AM", "am_format"},
    {"Printing_Formatting_PM", "pm_format"}};
constexpr FontDescriptor kHeaderPositions[] = {{"Printing_HeaderPos_Left", "head_left"},
    {"Printing_HeaderPos_Right", "head_right"}, {"Printing_HeaderPos_Top", "head_top"},
    {"Printing_HeaderPos_Bottom", "head_bottom"}};

core::JwpConfigurationKey font_key(std::size_t role, std::size_t field) {
  return {std::string(kFonts[role].name) + '.' + kFontFields[field].name,
          std::string(kFonts[role].alias) + '.' + kFontFields[field].alias};
}

}  // namespace

ApplicationSettings::ApplicationSettings() {
  fonts[static_cast<std::size_t>(JapaneseFontRole::kBig)].size = 48;
}

ApplicationSettings read_application_settings(std::string_view text,
                                               const ApplicationSettings& base) {
  const auto entries = core::parse_jwp_configuration(text);
  ApplicationSettings result = base;
  result.source = std::string(text);
  result.unapplied.clear();
  for (const auto& entry : entries) {
    std::string name;
    try {
      if (core::JwpConfigurationKey{"Printing_Justify_ASCII", "print_justify"}.matches(entry.name)) {
        name = "Printing_Justify_ASCII";
        result.print_formatting.justify_ascii = core::parse_jwp_setting_bool(entry.value);
      }
      for (std::size_t i = 0; i < 4; ++i) {
        if (core::JwpConfigurationKey{kPrintPatterns[i].name, kPrintPatterns[i].alias}.matches(entry.name)) {
          name = kPrintPatterns[i].name;
          auto& pattern = result.print_formatting.patterns[i];
          const auto bytes = core::parse_jwp_setting_bytes(entry.value, pattern.size() * 2);
          for (std::size_t j = 0; j < pattern.size(); ++j)
            pattern[j] = static_cast<core::JisCode>(bytes[j * 2] | (bytes[j * 2 + 1] << 8));
          core::validate_print_formatting(result.print_formatting);
        } else if (core::JwpConfigurationKey{kHeaderPositions[i].name, kHeaderPositions[i].alias}.matches(entry.name)) {
          name = kHeaderPositions[i].name;
          result.print_formatting.position[i] = static_cast<int>(core::parse_jwp_setting_integer(entry.value, 0, 1000));
        }
      }
      for (std::size_t role = 0; role < std::size(kFonts) && name.empty(); ++role) {
        for (std::size_t field = 0; field < std::size(kFontFields); ++field) {
          const auto key = font_key(role, field);
          if (!key.matches(entry.name)) continue;
          name = key.name;
          const bool printing = role == result.fonts.size();
          const bool ascii = role == result.fonts.size() + 1;
          auto& font = ascii ? result.ascii_font : printing ? result.print_font : result.fonts[role];
          if (field == 0) font.family = to_qstring(core::parse_jwp_setting_string(entry.value, 40));
          else if (field == 1) font.size = static_cast<int>(core::parse_jwp_setting_integer(entry.value, printing ? 10 : ascii ? 0 : 1, printing ? 1440 : 1024));
          else font.automatic = core::parse_jwp_setting_bool(entry.value);
          break;
        }
      }
      for (const auto& setting : kBooleans) {
        if (!name.empty()) break;
        if (!core::JwpConfigurationKey{setting.name, setting.alias}.matches(entry.name)) continue;
        name = setting.name;
        result.*(setting.member) = core::parse_jwp_setting_bool(entry.value);
      }
      for (const auto& setting : kDictionaryBooleans) {
        if (!name.empty()) break;
        if (!core::JwpConfigurationKey{setting.name, setting.alias}.matches(entry.name)) continue;
        name = setting.name;
        result.dictionary.*(setting.member) = core::parse_jwp_setting_bool(entry.value);
      }
      if (name.empty() && core::JwpConfigurationKey{"Dict_ExclusionFilters", "dict_bits"}.matches(entry.name)) {
        name = "Dict_ExclusionFilters";
        const auto bits = static_cast<std::uint32_t>(core::parse_jwp_setting_integer(entry.value,
            std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::uint32_t>::max()));
        result.dictionary.require_beginning = (bits & 1U) != 0;
        result.dictionary.require_end = (bits & 2U) != 0;
        result.dictionary.personal_names = (bits & 4U) == 0;
        result.dictionary.place_names = (bits & 8U) == 0;
        result.dictionary.category_exclusions = bits & core::kEdictCategoryMask;
        result.dictionary_extra_exclusions = bits & ~(core::kEdictCategoryMask | 15U);
      }
      if (name.empty() && core::JwpConfigurationKey{"TranslationCodePage", "code_page"}.matches(entry.name)) {
        name = "TranslationCodePage";
        const auto value = core::parse_jwp_setting_integer(entry.value, 0, 1258);
        if (value != 0 && value < 1250) throw core::JwpConfigurationError("Unknown code page");
        result.translation_code_page = static_cast<int>(value);
      }
      if (name.empty() && core::JwpConfigurationKey{"HistoryBuffers_NumChars", "history_size"}.matches(entry.name)) {
        name = "HistoryBuffers_NumChars";
        result.history_size = static_cast<int>(core::parse_jwp_setting_integer(entry.value, 0, 30000));
      }
      if (name.empty() && core::JwpConfigurationKey{"ToolbarButtons", "buttons"}.matches(entry.name)) {
        name = "ToolbarButtons";
        const auto bytes = core::parse_jwp_setting_bytes(entry.value, result.toolbar.buttons.size());
        std::copy(bytes.begin(), bytes.end(), result.toolbar.buttons.begin());
      }
      const struct { const char* key; const char* alias; int ToolbarSettings::*member; int min; int max; } toolbar_values[] = {
          {"ToolbarButtonCount", "button_count", &ToolbarSettings::count, 0, 100},
          {"Jwpqt_ToolbarArea", "", &ToolbarSettings::area, 0, 3},
          {"Jwpqt_ToolbarIconSize", "", &ToolbarSettings::icon_size, 16, 48},
          {"Jwpqt_ToolbarTextStyle", "", &ToolbarSettings::text_style, 0, 3}};
      for (const auto& value : toolbar_values) {
        if (!name.empty() || !core::JwpConfigurationKey{value.key, value.alias}.matches(entry.name)) continue;
        name = value.key;
        result.toolbar.*(value.member) = static_cast<int>(core::parse_jwp_setting_integer(entry.value, value.min, value.max));
      }
      if (name.empty() && core::JwpConfigurationKey{"Jwpqt_ToolbarLocked", ""}.matches(entry.name)) {
        name = "Jwpqt_ToolbarLocked";
        result.toolbar.locked = core::parse_jwp_setting_bool(entry.value);
      }
      if (name.empty() && core::JwpConfigurationKey{"CharInfo_Fields", "kanji_info"}.matches(entry.name)) {
        name = "CharInfo_Fields";
        const auto bytes = core::parse_jwp_setting_bytes(entry.value, result.kanji_info.fields.size());
        std::copy(bytes.begin(), bytes.end(), result.kanji_info.fields.begin());
        validate_kanji_info_options(result.kanji_info);
      }
      if (name.empty() && core::JwpConfigurationKey{"CharInfo_Compact", "info_compress"}.matches(entry.name)) {
        name = "CharInfo_Compact";
        result.kanji_info.compact = core::parse_jwp_setting_bool(entry.value);
      }
      if (name.empty() && core::JwpConfigurationKey{"CharInfo_ShowHeadings", "info_titles"}.matches(entry.name)) {
        name = "CharInfo_ShowHeadings";
        result.kanji_info.headings = core::parse_jwp_setting_bool(entry.value);
      }
    } catch (const std::invalid_argument& error) {
      throw core::JwpConfigurationError("Configuration line " + std::to_string(entry.line) +
          ", " + name + ": " + error.what());
    } catch (const core::JwpConfigurationError& error) {
      throw core::JwpConfigurationError("Configuration line " + std::to_string(entry.line) +
          ", " + name + ": " + error.what());
    }
    if (name.empty()) {
      const auto unknown = QString::fromLatin1(entry.name.data(), static_cast<qsizetype>(entry.name.size()));
      if (!result.unapplied.contains(unknown)) result.unapplied.push_back(unknown);
    }
  }
  if (result.dictionary_extra_exclusions != 0) {
    result.unapplied.push_back(QStringLiteral("Dict_ExclusionFilters (unsupported bits: 0x%1)")
        .arg(result.dictionary_extra_exclusions, 0, 16));
  }
  validate_toolbar(result.toolbar);
  return result;
}

std::string write_application_settings(const ApplicationSettings& settings) {
  try { core::validate_print_formatting(settings.print_formatting); }
  catch (const std::invalid_argument& error) { throw core::JwpConfigurationError(error.what()); }
  validate_toolbar(settings.toolbar);
  if (settings.history_size < 0 || settings.history_size > 30000)
    throw core::JwpConfigurationError("History storage is outside 0..30000 cells");
  if (settings.translation_code_page != 0 &&
      (settings.translation_code_page < 1250 || settings.translation_code_page > 1258)) {
    throw core::JwpConfigurationError("Unknown translation code page");
  }
  std::vector<core::JwpConfigurationUpdate> updates;
  updates.push_back({{"Printing_Justify_ASCII", "print_justify"}, settings.print_formatting.justify_ascii ? "true" : "false"});
  std::string toolbar_bytes;
  constexpr char digits[] = "0123456789ABCDEF";
  for (std::size_t i = 0; i < 4; ++i) {
    std::string bytes;
    for (auto code : settings.print_formatting.patterns[i]) {
      for (unsigned byte : {static_cast<unsigned>(code & 255), static_cast<unsigned>(code >> 8)}) {
        bytes.push_back(digits[byte >> 4]); bytes.push_back(digits[byte & 15]);
      }
    }
    updates.push_back({{kPrintPatterns[i].name, kPrintPatterns[i].alias}, bytes});
    updates.push_back({{kHeaderPositions[i].name, kHeaderPositions[i].alias}, std::to_string(settings.print_formatting.position[i])});
  }
  for (const auto byte : settings.toolbar.buttons) {
    toolbar_bytes.push_back(digits[byte >> 4]);
    toolbar_bytes.push_back(digits[byte & 15]);
  }
  updates.push_back({{"ToolbarButtons", "buttons"}, toolbar_bytes});
  updates.push_back({{"ToolbarButtonCount", "button_count"}, std::to_string(settings.toolbar.count)});
  updates.push_back({{"Jwpqt_ToolbarArea", ""}, std::to_string(settings.toolbar.area)});
  updates.push_back({{"Jwpqt_ToolbarIconSize", ""}, std::to_string(settings.toolbar.icon_size)});
  updates.push_back({{"Jwpqt_ToolbarTextStyle", ""}, std::to_string(settings.toolbar.text_style)});
  updates.push_back({{"Jwpqt_ToolbarLocked", ""}, settings.toolbar.locked ? "true" : "false"});
  for (std::size_t role = 0; role < std::size(kFonts); ++role) {
    const bool printing = role == settings.fonts.size();
    const bool ascii = role == settings.fonts.size() + 1;
    const auto& font = ascii ? settings.ascii_font : printing ? settings.print_font : settings.fonts[role];
    if (font.size < (printing ? 10 : ascii ? 0 : 1) || font.size > (printing ? 1440 : 1024))
      throw core::JwpConfigurationError("Font size is outside its supported range");
    if (font.family.size() > 39) throw core::JwpConfigurationError("Font name exceeds its legacy array");
    const auto family = from_qstring(font.family);
    if (to_qstring(family) != font.family) throw core::JwpConfigurationError("Font name is not valid Unicode");
    updates.push_back({font_key(role, 0), core::encode_jwp_setting_string(family, 40)});
    updates.push_back({font_key(role, 1), std::to_string(font.size)});
    updates.push_back({font_key(role, 2), font.automatic ? "true" : "false"});
  }
  for (const auto& setting : kBooleans) {
    updates.push_back({{setting.name, setting.alias}, settings.*(setting.member) ? "true" : "false"});
  }
  for (const auto& setting : kDictionaryBooleans) {
    updates.push_back({{setting.name, setting.alias}, settings.dictionary.*(setting.member) ? "true" : "false"});
  }
  if ((settings.dictionary_extra_exclusions & (core::kEdictCategoryMask | 15U)) != 0)
    throw core::JwpConfigurationError("Unsupported dictionary mask overlaps implemented filters");
  const auto& dictionary = settings.dictionary;
  if ((dictionary.category_exclusions & ~core::kEdictCategoryMask) != 0)
    throw core::JwpConfigurationError("Unknown dictionary category exclusion bits");
  const std::uint32_t bits = settings.dictionary_extra_exclusions | dictionary.category_exclusions |
      (dictionary.require_beginning ? 1U : 0U) | (dictionary.require_end ? 2U : 0U) |
      (dictionary.personal_names ? 0U : 4U) | (dictionary.place_names ? 0U : 8U);
  updates.push_back({{"Dict_ExclusionFilters", "dict_bits"},
                    "0x" + QString::number(bits, 16).toStdString()});
  updates.push_back({{"TranslationCodePage", "code_page"}, std::to_string(settings.translation_code_page)});
  updates.push_back({{"HistoryBuffers_NumChars", "history_size"}, std::to_string(settings.history_size)});
  validate_kanji_info_options(settings.kanji_info);
  std::string fields;
  constexpr char hex[] = "0123456789ABCDEF";
  for (const auto field : settings.kanji_info.fields) {
    fields.push_back(hex[field >> 4U]);
    fields.push_back(hex[field & 15U]);
  }
  updates.push_back({{"CharInfo_Fields", "kanji_info"}, fields});
  updates.push_back({{"CharInfo_Compact", "info_compress"}, settings.kanji_info.compact ? "true" : "false"});
  updates.push_back({{"CharInfo_ShowHeadings", "info_titles"}, settings.kanji_info.headings ? "true" : "false"});
  return core::rewrite_jwp_configuration(settings.source, updates);
}

ApplicationSettings read_application_settings_file(const QString& path,
                                                     const ApplicationSettings& base) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    throw core::JwpConfigurationError("Could not read settings: " + file.errorString().toStdString());
  const auto limit = static_cast<qint64>(core::JwpConfigurationLimits{}.bytes);
  const QByteArray bytes = file.read(limit + 1);
  if (file.error() != QFileDevice::NoError || bytes.size() > limit || !file.atEnd())
    throw core::JwpConfigurationError("Settings could not be read within the size limit");
  return read_application_settings(
      std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())), base);
}

void write_application_settings_file(const QString& path,
                                     const ApplicationSettings& settings) {
  const std::string bytes = write_application_settings(settings);
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(bytes.data(), static_cast<qint64>(bytes.size())) !=
          static_cast<qint64>(bytes.size()) || !file.commit())
    throw core::JwpConfigurationError("Could not save settings: " + file.errorString().toStdString());
}

}  // namespace jwpqt::qt
