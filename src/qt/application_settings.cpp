// SPDX-License-Identifier: GPL-2.0-or-later

#include "application_settings.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

#include <QFile>
#include <QSaveFile>

#include "jwpqt/core/jwp_configuration.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

struct FontDescriptor {
  const char* name;
  const char* alias;
};

constexpr FontDescriptor kFonts[] = {{"System", "sys_font"}, {"Edit", "edit_font"},
    {"List", "list_font"}, {"KanjiBar", "bar_font"}, {"File", "file_font"},
    {"Big", "big_font"}, {"Table", "jis_font"}};
constexpr FontDescriptor kFontFields[] = {{"Font", "name"}, {"Size", "size"},
    {"Auto", "automatic"}};
static_assert(std::size(kFonts) == static_cast<std::size_t>(JapaneseFontRole::kCount));

template <typename Owner>
struct BooleanDescriptor {
  const char* name;
  const char* alias;
  bool Owner::*member;
};

constexpr BooleanDescriptor<ApplicationSettings> kBooleans[] = {
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
    {"Dict_AdvancedSearches", "dict_advanced", &EdictLookupOptions::advanced},
    {"Dict_ContingentSearches", "dict_contingent", &EdictLookupOptions::contingent},
    {"Dict_AlwaysAdvancedSearch", "dict_always", &EdictLookupOptions::advanced_always},
    {"Dict_Adv_KeepSearching", "dict_showall", &EdictLookupOptions::advanced_show_all},
    {"Dict_Adv_Try_I_Adjectives", "dict_iadj", &EdictLookupOptions::i_adjectives},
    {"Dict_ClassicalSupport", "dict_classical", &EdictLookupOptions::classical},
    {"Dict_JASCII_to_ASCII", "dict_jascii2ascii", &EdictLookupOptions::jascii_to_ascii},
    {"Dict_ASCII_MatchFullEntry", "dict_fullascii", &EdictLookupOptions::full_ascii}};

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
      for (std::size_t role = 0; role < std::size(kFonts) && name.empty(); ++role) {
        for (std::size_t field = 0; field < std::size(kFontFields); ++field) {
          const auto key = font_key(role, field);
          if (!key.matches(entry.name)) continue;
          name = key.name;
          auto& font = result.fonts[role];
          if (field == 0) font.family = to_qstring(core::parse_jwp_setting_string(entry.value, 40));
          else if (field == 1) font.size = static_cast<int>(core::parse_jwp_setting_integer(entry.value, 1, 1024));
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
        result.dictionary_extra_exclusions = bits & ~15U;
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
  return result;
}

std::string write_application_settings(const ApplicationSettings& settings) {
  if (settings.history_size < 0 || settings.history_size > 30000)
    throw core::JwpConfigurationError("History storage is outside 0..30000 cells");
  if (settings.translation_code_page != 0 &&
      (settings.translation_code_page < 1250 || settings.translation_code_page > 1258)) {
    throw core::JwpConfigurationError("Unknown translation code page");
  }
  std::vector<core::JwpConfigurationUpdate> updates;
  for (std::size_t role = 0; role < std::size(kFonts); ++role) {
    const auto& font = settings.fonts[role];
    if (font.size < 1 || font.size > 1024) throw core::JwpConfigurationError("Font size is outside 1..1024");
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
  if ((settings.dictionary_extra_exclusions & 15U) != 0)
    throw core::JwpConfigurationError("Unsupported dictionary mask overlaps implemented filters");
  const auto& dictionary = settings.dictionary;
  const std::uint32_t bits = settings.dictionary_extra_exclusions |
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
