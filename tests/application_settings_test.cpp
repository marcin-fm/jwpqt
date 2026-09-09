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

void test_history_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.history_size == 300 && defaults.save_histories, "History defaults differ from source");
  for (const int size : {0, 1, 3, 4, 12, 300, 30000}) {
    auto value = read_application_settings("history_size=1\nHistoryBuffers_NumChars=" + std::to_string(size) +
        "\nsave_history=no\nSAVE_HISTORY=bad\nFuture=retained\n");
    const auto encoded = write_application_settings(value);
    const auto restored = read_application_settings(encoded);
    require(restored.history_size == size && !restored.save_histories &&
                restored.unapplied == QStringList({"SAVE_HISTORY", "Future"}) &&
                encoded.find("history_size=") == std::string::npos &&
                write_application_settings(restored) == encoded,
            "History settings lost bounds, aliases or unknown data");
  }
  for (const auto* invalid : {"HistoryBuffers_NumChars=-1", "history_size=30001", "save_history=2",
       "history_size=bad\nHistoryBuffers_NumChars=300", "Save_Histories=bad\nsave_history=true"})
    rejects([&] { (void)read_application_settings(invalid); });
  auto invalid = defaults;
  for (const int size : {-1, 30001}) {
    invalid.history_size = size;
    rejects([&] { (void)write_application_settings(invalid); });
  }
}

void test_conversion_choice_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.conversion_choices == 200,
          "Conversion-choice default differs from the source");
  for (const int size : {10, 200, 2000}) {
    const auto settings = read_application_settings(
        "convert_size=10\nConversionChoicesStored=" + std::to_string(size));
    const auto encoded = write_application_settings(settings);
    require(settings.conversion_choices == size &&
                encoded.find("convert_size=") == std::string::npos &&
                read_application_settings(encoded).conversion_choices == size &&
                write_application_settings(read_application_settings(encoded)) == encoded,
            "Conversion-choice setting lost its bounds, alias, or canonical form");
  }
  for (const auto* invalid : {"ConversionChoicesStored=9", "convert_size=2001",
                              "ConversionChoicesStored=bad\nconvert_size=200"})
    rejects([&] { (void)read_application_settings(invalid); });
  auto invalid = defaults;
  for (const int size : {9, 2001}) {
    invalid.conversion_choices = size;
    rejects([&] { (void)write_application_settings(invalid); });
  }
}

void test_duplicate_open_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.duplicate_open == DuplicateOpenBehavior::kPrompt,
          "Duplicate-open default differs from the source");
  for (int value = 0; value <= 2; ++value) {
    const auto settings = read_application_settings(
        "double_open=0\nDoubleOpenBehavior=" + std::to_string(value));
    const auto expected = static_cast<DuplicateOpenBehavior>(value);
    const auto encoded = write_application_settings(settings);
    require(settings.duplicate_open == expected &&
                read_application_settings(encoded).duplicate_open == expected &&
                encoded.find("double_open=") == std::string::npos &&
                write_application_settings(read_application_settings(encoded)) == encoded,
            "Duplicate-open setting did not canonicalize and round-trip");
  }
  for (const auto* invalid : {"DoubleOpenBehavior=-1", "double_open=3",
                              "double_open=bad\nDoubleOpenBehavior=1"})
    rejects([&] { (void)read_application_settings(invalid); });
  auto invalid = defaults;
  invalid.duplicate_open = static_cast<DuplicateOpenBehavior>(3);
  rejects([&] { (void)write_application_settings(invalid); });
}

void test_clipboard_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.clipboard_export == ClipboardTextFormat::kShiftJis &&
              defaults.clipboard_import == ClipboardTextFormat::kAutoDetect &&
              !defaults.omit_clipboard_unicode,
          "Clipboard protocol defaults differ from the source");
  for (int value = 6; value <= 13; ++value) {
    const auto format = static_cast<ClipboardTextFormat>(value);
    auto settings = read_application_settings(
        "clip_write=6\nClipboard_Format_Export=" + std::to_string(value) +
        "\nclip_read=1\nClipboard_Format_Import=" + std::to_string(value) +
        "\nno_UNICODETEXT=true");
    const auto encoded = write_application_settings(settings);
    const auto restored = read_application_settings(encoded);
    require(settings.clipboard_export == format &&
                settings.clipboard_import == format &&
                settings.omit_clipboard_unicode &&
                restored.clipboard_export == format &&
                restored.clipboard_import == format &&
                restored.omit_clipboard_unicode &&
                encoded.find("clip_write=") == std::string::npos &&
                encoded.find("clip_read=") == std::string::npos &&
                write_application_settings(restored) == encoded,
            "Clipboard protocol setting did not canonicalize and round-trip");
  }
  for (const auto* invalid : {"Clipboard_Format_Export=1", "clip_write=2",
                              "Clipboard_Format_Import=5", "clip_read=14",
                              "no_UNICODETEXT=2",
                              "Clipboard_Format_Export=bad\nclip_write=7"})
    rejects([&] { (void)read_application_settings(invalid); });
  auto invalid = defaults;
  invalid.clipboard_export = ClipboardTextFormat::kAutoDetect;
  rejects([&] { (void)write_application_settings(invalid); });
  invalid = defaults;
  invalid.clipboard_import = static_cast<ClipboardTextFormat>(2);
  rejects([&] { (void)write_application_settings(invalid); });
}

void test_autoscroll_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.auto_scroll && defaults.auto_scroll_speed == 100,
          "Autoscroll defaults differ from the source");
  const auto settings = read_application_settings(
      "auto_scroll=true\nAutoScroll=false\nscroll_speed=5\nAutoScroll_Speed=250");
  const auto encoded = write_application_settings(settings);
  require(!settings.auto_scroll && settings.auto_scroll_speed == 250 &&
              encoded.find("auto_scroll") == std::string::npos &&
              encoded.find("scroll_speed") == std::string::npos &&
              read_application_settings(encoded).auto_scroll_speed == 250 &&
              !read_application_settings(encoded).auto_scroll,
          "Autoscroll settings did not canonicalize and round-trip");
  for (const auto* invalid : {"AutoScroll_Speed=-1", "scroll_speed=10001",
                              "scroll_speed=bad\nAutoScroll_Speed=100"})
    rejects([&] { (void)read_application_settings(invalid); });
  auto invalid = defaults;
  invalid.auto_scroll_speed = 10001;
  rejects([&] { (void)write_application_settings(invalid); });
}

void test_metric_unit_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(!defaults.metric_units,
          "Measurement-unit default differs from the source");

  const auto settings = read_application_settings(
      "units_cm=false\nMetricUnits=true\nFuture_Units=preserved");
  const auto encoded = write_application_settings(settings);
  require(settings.metric_units &&
              read_application_settings(encoded).metric_units &&
              encoded.find("units_cm") == std::string::npos &&
              encoded.find("MetricUnits = true") != std::string::npos &&
              encoded.find("Future_Units=preserved") != std::string::npos &&
              write_application_settings(read_application_settings(encoded)) == encoded,
          "Measurement-unit setting did not canonicalize and round-trip");

  for (const auto* invalid : {"MetricUnits=maybe",
                              "units_cm=bad\nMetricUnits=false"})
    rejects([&] { (void)read_application_settings(invalid); });
}

void test_line_width_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.line_width_mode == LineWidthMode::kDynamic &&
              defaults.fixed_line_width == 35,
          "Document line-width defaults differ from the source");
  for (int mode = 0; mode <= 2; ++mode) {
    const auto settings = read_application_settings(
        "width_mode=0\nLineWidth_Mode=" + std::to_string(mode) +
        "\nchar_width=5\nLineWidth_Fixed=1000\nFuture_Width=retained");
    const auto encoded = write_application_settings(settings);
    require(settings.line_width_mode == static_cast<LineWidthMode>(mode) &&
                settings.fixed_line_width == 1000 &&
                read_application_settings(encoded).line_width_mode ==
                    static_cast<LineWidthMode>(mode) &&
                read_application_settings(encoded).fixed_line_width == 1000 &&
                encoded.find("width_mode=") == std::string::npos &&
                encoded.find("char_width=") == std::string::npos &&
                encoded.find("Future_Width=retained") != std::string::npos &&
                write_application_settings(read_application_settings(encoded)) == encoded,
            "Document line-width settings did not canonicalize and round-trip");
  }
  for (const auto* invalid : {"LineWidth_Mode=-1", "width_mode=3",
                               "LineWidth_Fixed=4", "char_width=1001",
                               "width_mode=bad\nLineWidth_Mode=0",
                               "char_width=bad\nLineWidth_Fixed=35"})
    rejects([&] { (void)read_application_settings(invalid); });
  auto invalid = defaults;
  invalid.line_width_mode = static_cast<LineWidthMode>(3);
  rejects([&] { (void)write_application_settings(invalid); });
  invalid = defaults;
  invalid.fixed_line_width = 4;
  rejects([&] { (void)write_application_settings(invalid); });
}

void test_margin_relaxation_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.relax_margin_punctuation &&
              defaults.relax_margin_small_kana,
          "Margin-relaxation defaults differ from the source");
  const auto settings = read_application_settings(
      "relax_punctuation=true\nRelaxMargins_Punctuation=false\n"
      "relax_smallkana=true\nRelaxMargins_SmallKana=false\n"
      "Future_Relax=retained");
  const auto encoded = write_application_settings(settings);
  require(!settings.relax_margin_punctuation &&
              !settings.relax_margin_small_kana &&
              encoded.find("relax_punctuation=") == std::string::npos &&
              encoded.find("relax_smallkana=") == std::string::npos &&
              encoded.find("RelaxMargins_Punctuation = false") !=
                  std::string::npos &&
              encoded.find("RelaxMargins_SmallKana = false") !=
                  std::string::npos &&
              encoded.find("Future_Relax=retained") != std::string::npos &&
              write_application_settings(read_application_settings(encoded)) ==
                  encoded,
          "Margin-relaxation settings did not canonicalize and round-trip");
  for (const auto* invalid : {
           "RelaxMargins_Punctuation=maybe",
           "relax_punctuation=bad\nRelaxMargins_Punctuation=true",
           "RelaxMargins_SmallKana=maybe",
           "relax_smallkana=bad\nRelaxMargins_SmallKana=true"})
    rejects([&] { (void)read_application_settings(invalid); });
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

void test_dictionary_settings() {
  using namespace jwpqt::qt;
  const auto defaults = read_application_settings("");
  require(defaults.dictionary.require_beginning && !defaults.dictionary.require_end &&
              !defaults.dictionary.personal_names && !defaults.dictionary.place_names &&
              !defaults.dictionary.advanced && defaults.dictionary.advanced_always &&
              defaults.dictionary.i_adjectives && !defaults.dictionary.classical &&
               !defaults.dictionary.full_ascii && !defaults.dictionary.jascii_to_ascii &&
                !defaults.dictionary.contingent && defaults.dictionary.automatic_search && !defaults.dictionary.compact &&
               !defaults.dictionary.link_advanced_names && defaults.dictionary.priority_first &&
               defaults.dictionary.priority_separator && defaults.dictionary.advanced_separator && !defaults.dictionary.monitor_clipboard,
          "Dictionary defaults differ from the source");
  auto settings = read_application_settings(
      "dict_advanced=no\nDiCt_AdvancedSearches=yes\ndict_always=false\n"
      "dict_showall=true\ndict_iadj=false\ndict_classical=true\ndict_contingent=yes\ndict_auto=false\n"
      "dict_fullascii=true\ndict_jascii2ascii=true\ndict_bits=0x80000006\ndict_compress=yes\n"
        "dict_link_adv_noname=yes\ndict_watchclip=yes\nDICT_BITS=bad\nFuture_Presentation=true\nFuture=\xff\n");
  require(settings.dictionary.link_advanced_names && settings.dictionary.compact && !settings.dictionary.automatic_search && settings.dictionary.contingent && settings.dictionary.advanced && !settings.dictionary.advanced_always &&
              settings.dictionary.advanced_show_all && !settings.dictionary.i_adjectives &&
              settings.dictionary.classical && settings.dictionary.full_ascii &&
              settings.dictionary.jascii_to_ascii && !settings.dictionary.require_beginning &&
              settings.dictionary.require_end && !settings.dictionary.personal_names &&
              settings.dictionary.place_names && settings.dictionary_extra_exclusions == 0x80000000U &&
              settings.unapplied.size() == 4 &&
              settings.unapplied.back().contains(QStringLiteral("0x80000000")),
          "Dictionary aliases, mask inversion or unsupported-field reporting changed");
  const auto encoded = write_application_settings(settings);
  const auto restored = read_application_settings(encoded);
  require(restored.dictionary.monitor_clipboard && encoded.find("Dict_MonitorClipboard = true") != std::string::npos &&
               restored.dictionary.link_advanced_names && restored.dictionary.advanced && restored.dictionary.place_names &&
              restored.dictionary.compact && restored.dictionary.contingent && write_application_settings(restored) == encoded &&
              encoded.find("Dict_ExclusionFilters = 0x80000006") != std::string::npos &&
              encoded.find("DICT_BITS=bad\n") != std::string::npos &&
               encoded.find("Future_Presentation=true\n") != std::string::npos &&
              encoded.find("Future=\xff\n") != std::string::npos &&
              encoded.find("dict_advanced=") == std::string::npos,
          "Dictionary settings did not preserve unknown values and canonicalize aliases");
  const auto migrated = read_application_settings(
      "  MonitorClipboard=false\r\nDict_MonitorClipboard=true\nmonitorclipboard=false\r"
      "dict_watchclip=yes\nFuture_Field=\xff\n");
  require(migrated.dictionary.monitor_clipboard && migrated.unapplied == QStringList{QStringLiteral("Future_Field")},
          "Clipboard policy source/native aliases did not follow source order");
  const auto migration = write_application_settings(migrated);
  int clipboard_keys = 0;
  for (const auto& entry : jwpqt::core::parse_jwp_configuration(migration)) {
    require(!jwpqt::core::JwpConfigurationKey{"MonitorClipboard", "dict_watchclip"}.matches(entry.name),
            "Old native clipboard key survived canonical save");
    if (entry.name == "Dict_MonitorClipboard") ++clipboard_keys;
  }
  require(clipboard_keys == 1 && migration.find("Future_Field=\xff\n") != std::string::npos &&
              read_application_settings(migration).dictionary.monitor_clipboard &&
              write_application_settings(read_application_settings(migration)) == migration,
          "Clipboard policy migration lost unknown data or duplicated the canonical key");
  require(read_application_settings("dIcT_mOnItOrClIpBoArD=true").dictionary.monitor_clipboard &&
              !read_application_settings("MonitorClipboard=true\nDict_MonitorClipboard=false").dictionary.monitor_clipboard &&
              read_application_settings("Dict_MonitorClipboard=false\nMonitorClipboard=true").dictionary.monitor_clipboard,
          "Source clipboard policy or ordered native compatibility changed");
  for (unsigned bits = 0; bits < 16; ++bits) {
    const auto value = read_application_settings("Dict_ExclusionFilters=" + std::to_string(bits));
    require(value.dictionary.require_beginning == ((bits & 1U) != 0) &&
                value.dictionary.require_end == ((bits & 2U) != 0) &&
                value.dictionary.personal_names == ((bits & 4U) == 0) &&
                value.dictionary.place_names == ((bits & 8U) == 0) &&
                value.dictionary_extra_exclusions == 0 && value.unapplied.isEmpty() &&
                read_application_settings(write_application_settings(value)).dictionary.personal_names ==
                    value.dictionary.personal_names,
            "A supported filter mask changed or was reported unsupported");
  }
  for (const auto* mask : {"-1", "0xFFFFFFFF", "4294967295"}) {
    const auto all = read_application_settings(std::string("dict_bits=") + mask);
    require(all.dictionary_extra_exclusions == 0xfe000000U &&
                all.dictionary.category_exclusions == 0x01fffff0U && all.dictionary.require_end &&
                !all.dictionary.personal_names && !all.dictionary.place_names,
            "A source signed/unsigned 32-bit mask was truncated");
  }
  require(read_application_settings("dict_bits=0x100\ndict_bits=13").unapplied.isEmpty(),
          "Overridden unsupported mask bits produced a stale warning");
  for (const auto* bad : {"Dict_AdvancedSearches=bad\nDict_AdvancedSearches=true",
                            "Dict_MonitorClipboard=bad\nMonitorClipboard=true",
                           "MonitorClipboard=bad\ndict_watchclip=true", "dict_watchclip=2",
                          "Dict_ContingentSearches=bad\nDict_ContingentSearches=true",
                          "dict_contingent=2",
                          "Dict_AutoSearch=bad\nDict_AutoSearch=true", "dict_auto=2",
                          "Dict_Compact=bad\ndict_compress=true", "dict_compress=2",
                          "Dict_Link_Adv_NoNames=bad\ndict_link_adv_noname=true", "dict_link_adv_noname=2",
                         "dict_always=2", "dict_showall=on", "dict_iadj=maybe",
                         "dict_classical=2", "dict_jascii2ascii=2", "dict_fullascii=2",
                         "dict_bits=bad\ndict_bits=13", "dict_bits=4294967296",
                         "dict_bits=-2147483649", "dict_bits=1.5"}) {
    rejects([&] { (void)read_application_settings(bad, settings); });
  }
  const auto overlay = read_application_settings("Dict_AdvancedSearches=false", settings);
  const auto presentation = read_application_settings("dict_primaryfirst=false\ndict_primark=no\ndict_advmark=false");
  const auto presentation_copy = read_application_settings(write_application_settings(presentation));
  require(!presentation_copy.dictionary.priority_first && !presentation_copy.dictionary.priority_separator &&
          !presentation_copy.dictionary.advanced_separator, "Presentation aliases did not persist");
  for (const auto* bad : {"Dict_PriorityEntriesFirst=bad\ndict_primaryfirst=true",
                          "dict_primark=2", "Dict_Adv_SeparatorMark=bad\ndict_advmark=true"})
    rejects([&] { (void)read_application_settings(bad); });
  require(!overlay.dictionary.advanced && settings.dictionary.advanced &&
              overlay.dictionary_extra_exclusions == settings.dictionary_extra_exclusions &&
              overlay.dictionary.require_end && overlay.dictionary.place_names,
          "A dictionary overlay mutated its base or reset unspecified filters");
  settings.dictionary_extra_exclusions |= 1U;
  rejects([&] { (void)write_application_settings(settings); });
  for (unsigned i = 4; i <= 24; ++i) {
    const auto bit = std::uint32_t{1} << i;
    auto category = read_application_settings("dict_bits=" + std::to_string(bit | 13U));
    require(category.dictionary.category_exclusions == bit && category.unapplied.isEmpty() &&
                category.dictionary_extra_exclusions == 0 && category.dictionary.require_beginning &&
                !category.dictionary.personal_names && !category.dictionary.place_names,
            "A newly supported category was lost, inverted or still reported unsupported");
    const auto bytes = write_application_settings(category);
    require(read_application_settings(bytes).dictionary.category_exclusions == bit &&
                write_application_settings(read_application_settings(bytes)) == bytes,
            "A category exclusion failed canonical persistence");
    const auto overlaid = read_application_settings("Dict_Compact=true", category);
    require(overlaid.dictionary.category_exclusions == bit, "Unrelated import reset a category");
    category.dictionary_extra_exclusions = bit;
    rejects([&] { (void)write_application_settings(category); });
  }
  for (const auto bit : {1U, 8U, 0x02000000U, 0x80000000U}) {
    auto invalid = defaults;
    invalid.dictionary.category_exclusions = bit;
    rejects([&] { (void)write_application_settings(invalid); });
  }
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  try {
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create temporary settings directory");
    test_model();
    test_history_settings();
    test_conversion_choice_settings();
    test_duplicate_open_settings();
    test_clipboard_settings();
    test_autoscroll_settings();
    test_metric_unit_settings();
    test_line_width_settings();
    test_margin_relaxation_settings();
    test_information_settings();
    test_dictionary_settings();
    test_files(directory.path());
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
