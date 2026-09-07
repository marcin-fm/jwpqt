// SPDX-License-Identifier: GPL-2.0-or-later

#include "application_settings.h"

#include <iterator>
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

struct BooleanDescriptor {
  const char* name;
  const char* alias;
  bool ApplicationSettings::*member;
};

constexpr BooleanDescriptor kBooleans[] = {
    {"Show_Toolbar", "toolbar", &ApplicationSettings::show_toolbar},
    {"Show_StatusBar", "status", &ApplicationSettings::show_status_bar},
    {"Show_KanjiBar", "kanjibar", &ApplicationSettings::show_kanji_bar},
    {"KanjiBarAtTop", "kanjibar_top", &ApplicationSettings::kanji_bar_at_top},
    {"ScrollBar_Vertical", "vscroll", &ApplicationSettings::vertical_scrollbar},
    {"ScrollBar_Horizontal", "hscroll", &ApplicationSettings::horizontal_scrollbar},
    {"ScrollBar_KanjiBar", "kscroll", &ApplicationSettings::kanji_bar_scrollbar},
    {"SaveSettingsOnExit", "save_exit", &ApplicationSettings::save_settings_on_exit},
    {"Save_RecentFiles", "save_recent", &ApplicationSettings::save_recent_files}};

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
      if (name.empty() && core::JwpConfigurationKey{"TranslationCodePage", "code_page"}.matches(entry.name)) {
        name = "TranslationCodePage";
        const auto value = core::parse_jwp_setting_integer(entry.value, 0, 1258);
        if (value != 0 && value < 1250) throw core::JwpConfigurationError("Unknown code page");
        result.translation_code_page = static_cast<int>(value);
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
  return result;
}

std::string write_application_settings(const ApplicationSettings& settings) {
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
  updates.push_back({{"TranslationCodePage", "code_page"}, std::to_string(settings.translation_code_page)});
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
