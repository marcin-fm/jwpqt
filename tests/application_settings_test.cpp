// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>
#include <string>

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include "application_settings.h"
#include "jwpqt/core/jwp_configuration.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void rejects(Function operation) {
  try { operation(); }
  catch (const jwpqt::core::JwpConfigurationError&) { return; }
  throw std::runtime_error("Invalid application settings were accepted");
}

QByteArray bytes_at(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Could not read test settings");
  return file.readAll();
}

void test_model() {
  using namespace jwpqt::qt;
  constexpr auto file = static_cast<std::size_t>(JapaneseFontRole::kFile);
  const std::string source = "# retained\r\nShow_Toolbar=false\nfile_font.size=19\r"
      "fIlE.sIzE=20\nFile.Auto=no\nFuture = \xff\nFILE_FONT.size=bad\n";
  auto settings = read_application_settings(source);
  require(!settings.show_toolbar && settings.fonts[file].size == 20 &&
          !settings.fonts[file].automatic && settings.source == source &&
          settings.unapplied == QStringList({QStringLiteral("Future"), QStringLiteral("FILE_FONT.size")}),
          "Settings aliases, ordered values or unknown source were lost");
  settings.fonts[file].family = QString(QChar(0xfeff)) + QStringLiteral("\u65e5\U0001f600");
  settings.save_recent_files = false;
  settings.save_settings_on_exit = false;
  settings.kanji_bar_at_top = true;
  settings.translation_code_page = 1251;
  const auto encoded = write_application_settings(settings);
  const auto restored = read_application_settings(encoded);
  require(restored.fonts[file].family == settings.fonts[file].family &&
          restored.fonts[file].size == 20 && !restored.fonts[file].automatic &&
          !restored.show_toolbar && !restored.save_recent_files &&
          !restored.save_settings_on_exit && restored.kanji_bar_at_top &&
          restored.translation_code_page == 1251 &&
          encoded.find("Future = \xff\n") != std::string::npos &&
          encoded.find("FILE_FONT.size=bad\n") != std::string::npos &&
          encoded.find("file_font.size=") == std::string::npos &&
          write_application_settings(restored) == encoded,
          "Native settings did not round-trip without losing retained fields");
  for (int code_page : {0, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258}) {
    settings.translation_code_page = code_page;
    require(read_application_settings(write_application_settings(settings)).translation_code_page == code_page,
            "A supported translation code page did not round-trip");
  }
  for (const auto* bad : {"File.Size=bad\nFile.Size=18\n", "File.Size=0", "File.Size=1025",
                         "File.Auto=2", "Show_StatusBar=on", "TranslationCodePage=1",
                         "code_page=1249", "code_page=1259", "File.Font=00D80000"})
    rejects([&] { (void)read_application_settings(bad, settings); });
  const auto overlay = read_application_settings("Show_StatusBar=false", settings);
  require(!overlay.show_status_bar && overlay.fonts[file].family == settings.fonts[file].family &&
          settings.show_status_bar, "Import overlay mutated its base or reset unspecified values");
  settings.fonts[file].family = QString(39, QLatin1Char('a'));
  (void)write_application_settings(settings);
  settings.fonts[file].family += QLatin1Char('a');
  rejects([&] { (void)write_application_settings(settings); });
  settings.fonts[file].family = QString(QChar(0xd800));
  rejects([&] { (void)write_application_settings(settings); });
}

void test_files(const QString& directory) {
  using namespace jwpqt::qt;
  const QString path = directory + QStringLiteral("/settings-\u65e5.cfg");
  auto settings = read_application_settings("Unknown = retained\r\nFile.Size=24");
  write_application_settings_file(path, settings);
  const auto bytes = bytes_at(path);
  require(read_application_settings_file(path).fonts[static_cast<std::size_t>(JapaneseFontRole::kFile)].size == 24,
          "Settings file did not load");
  settings.translation_code_page = 9;
  rejects([&] { write_application_settings_file(path, settings); });
  require(bytes_at(path) == bytes, "Invalid settings overwrote the previous file");
  rejects([&] { (void)read_application_settings_file(directory); });
  rejects([&] { (void)read_application_settings_file(directory + QStringLiteral("/missing.cfg")); });
  settings.translation_code_page = 1252;
  rejects([&] { write_application_settings_file(directory + QStringLiteral("/missing/settings.cfg"), settings); });
  QFile oversized(path);
  require(oversized.open(QIODevice::WriteOnly | QIODevice::Truncate), "Could not create large settings");
  const QByteArray data(1024 * 1024 + 1, '#');
  require(oversized.write(data) == data.size(), "Could not write large settings");
  oversized.close();
  rejects([&] { (void)read_application_settings_file(path); });
  require(bytes_at(path) == data, "Invalid settings input was rewritten");
}

void test_information_settings() {
  using namespace jwpqt::qt;
  ApplicationSettings settings;
  require(settings.kanji_info.fields[6] == 14 && settings.kanji_info.fields[13] == 7 &&
          settings.kanji_info.fields[59] == 60, "Character information defaults differ from desktop source");
  select_kanji_info_field(settings.kanji_info, 0, 17);
  require(settings.kanji_info.fields[0] == 17 && settings.kanji_info.fields[16] == 1,
          "A duplicate field was not replaced with the first missing field");
  select_kanji_info_field(settings.kanji_info, 1, 0);
  select_kanji_info_field(settings.kanji_info, 2, 0);
  select_kanji_info_field(settings.kanji_info, 1, 17);
  require(settings.kanji_info.fields[0] == 2 && settings.kanji_info.fields[1] == 17 &&
          settings.kanji_info.fields[2] == 0, "Blank fields or source duplicate repair changed");
  settings.kanji_info.fields[59] = 255;
  settings.kanji_info.compact = true;
  settings.kanji_info.headings = false;
  const auto before = settings.kanji_info;
  rejects([&] { select_kanji_info_field(settings.kanji_info, 26, 1); });
  rejects([&] { select_kanji_info_field(settings.kanji_info, 1, 27); });
  require(settings.kanji_info == before, "Invalid field selection mutated information settings");
  const auto bytes = write_application_settings(settings);
  const auto restored = read_application_settings(bytes);
  require(restored.kanji_info == before && write_application_settings(restored) == bytes,
          "Information settings or reserved tail did not round-trip");
  const auto alias = read_application_settings("info_compress=no\ninfo_titles=yes\nCharInfo_SingleDialog=true", settings);
  require(!alias.kanji_info.compact && alias.kanji_info.headings &&
          alias.unapplied == QStringList{QStringLiteral("CharInfo_SingleDialog")} &&
          settings.kanji_info == before, "Information overlay changed its base or applied singleton behavior");
  rejects([&] { (void)read_application_settings("CharInfo_Fields=00", settings); });
  rejects([&] { (void)read_application_settings("CharInfo_Compact=bad\nCharInfo_Compact=true", settings); });
  const std::string invalid = "1B" + std::string(118, '0');
  rejects([&] { (void)read_application_settings("kanji_info=" + invalid + "\n" + bytes, settings); });
  settings.kanji_info.fields[25] = 27;
  rejects([&] { (void)write_application_settings(settings); });
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  try {
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create temporary settings directory");
    test_model();
    test_information_settings();
    test_files(directory.path());
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
