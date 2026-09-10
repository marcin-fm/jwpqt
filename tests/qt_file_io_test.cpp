// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

#include "file_io.h"
#include "jwpqt/core/edict_registry.h"
#include "jwpqt/core/jfc_text.h"
#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/kanji_info.h"
#include "jwpqt/core/utf8.h"
#include "jwpqt/core/utf16.h"
#include "jwpqt/core/wnn_preferences.h"
#include "jwpqt/core/wnn_user_dictionary.h"
#include "text_bridge.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

QByteArray read_bytes(const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    throw std::runtime_error(input.errorString().toStdString());
  }
  return input.readAll();
}

void test_backups(const QString& directory) {
  using namespace jwpqt;
  const auto path = directory + QStringLiteral("/backup-愛.txt");
  const auto backup = path + QStringLiteral("_BAK");
  const core::TextFile first{U"first\ufeff愛", core::TextEncoding::kUtf16Be, true};
  const core::TextFile second{U"second", core::TextEncoding::kUtf8, false};
  qt::write_text_file(path, first, true);
  require(!QFile::exists(backup), "New file unexpectedly created a backup");
  const auto first_bytes = read_bytes(path);
  qt::write_text_file(path, second, true);
  require(read_bytes(backup) == first_bytes && qt::read_text_file(path, second.encoding).text == second.text,
          "Backup did not preserve exact prior encoding/BOM bytes");
  const auto second_bytes = read_bytes(path);
  qt::write_text_file(path, first, true);
  require(read_bytes(backup) == second_bytes, "Repeated save retained the wrong generation");
  qt::write_text_file(path, second);
  require(read_bytes(backup) == second_bytes, "Disabled backup policy changed backup");
  const auto fails = [](auto&& write) { try { write(); } catch (const std::exception&) { return true; } return false; };
  require(fails([&] { qt::write_text_file(path, {U"\U0001f600", core::TextEncoding::kEucJp, false}, true); }) &&
          read_bytes(path) == second_bytes && read_bytes(backup) == second_bytes,
          "Encoding failure changed source or backup");
  require(QFile::remove(backup) && QDir().mkdir(backup), "Backup directory fixture failed");
  require(fails([&] { qt::write_text_file(path, first, true); }) && read_bytes(path) == second_bytes,
          "Failed backup published new document");
  require(QDir().rmdir(backup) && QFile::link(path, backup), "Backup alias fixture failed");
  require(fails([&] { qt::write_text_file(path, first, true); }) && read_bytes(path) == second_bytes,
          "Backup followed an alias to its source");
  require(QFile::remove(backup), "Could not clear backup alias");
#ifdef Q_OS_UNIX
  require(::mkfifo(QFile::encodeName(backup).constData(), 0600) == 0, "Backup FIFO fixture failed");
  require(fails([&] { qt::write_text_file(path, first, true); }) && read_bytes(path) == second_bytes,
          "Backup replaced a special file");
  struct stat type {};
  require(::lstat(QFile::encodeName(backup).constData(), &type) == 0 && S_ISFIFO(type.st_mode), "Backup destroyed FIFO");
  require(QFile::remove(backup), "Could not clear backup FIFO");
#endif
  QFile large(path);
  const QByteArray original(2 * 1024 * 1024 + 3, 'x');
  require(large.open(QIODevice::WriteOnly) && large.write(original) == original.size(), "Large fixture failed");
  large.close();
  qt::write_text_file(path, second, true);
  require(read_bytes(backup) == original, "Streaming backup lost bytes across chunks");
  require(QFile::remove(path), "Remove source fixture failed");
  qt::write_text_file(path, first, true);
  require(read_bytes(backup) == original, "New target erased an existing backup");
  core::JwpDocument document;
  document.paragraphs.push_back({});
  document.paragraphs.front().text = {0x2422};
  qt::write_jwp_file(path, document, true);
  require(read_bytes(backup) == first_bytes && qt::read_jwp_file(path) == document,
          "JWP save did not back up old text bytes");
  const auto jwp_bytes = read_bytes(path);
  document.summary[0] = {0};
  require(fails([&] { qt::write_jwp_file(path, document, true); }) &&
          read_bytes(path) == jwp_bytes && read_bytes(backup) == first_bytes,
          "Invalid JWP changed source or backup");
}

void test_text_bridge() {
  using jwpqt::qt::from_qstring;
  using jwpqt::qt::to_qstring;
  const std::u32string signatures = U"\ufeff\ufeff\ufffe\u00a0\U0001f600";
  require(from_qstring(to_qstring(signatures)) == signatures,
          "Qt text bridge interpreted text as a Unicode file signature");
  std::u32string scalars;
  for (char32_t value = 0; value <= 0x10ffff; ++value) {
    if (value < 0xd800 || value > 0xdfff) scalars.push_back(value);
  }
  require(from_qstring(to_qstring(scalars)) == scalars,
          "Qt text bridge changed a Unicode scalar");
  for (const char32_t value : {char32_t{0xd800}, char32_t{0xdfff},
                              char32_t{0x110000}, char32_t{0xffffffff}}) {
    bool rejected = false;
    try {
      static_cast<void>(to_qstring(std::u32string(1, value)));
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    require(rejected, "Qt text bridge accepted a non-scalar Unicode value");
  }
}

void test_file_round_trip(const QString& directory,
                          jwpqt::core::TextEncoding encoding,
                          bool has_byte_order_mark) {
  const QString path =
      directory + QStringLiteral("/unicode-\u65e5\u672c\u8a9e.txt");
  const jwpqt::core::TextFile expected{
      U"\u65e5\u672c\u8a9e\nsecond line\n", encoding,
      has_byte_order_mark};

  jwpqt::qt::write_text_file(path, expected);
  const jwpqt::core::TextFile actual =
      jwpqt::qt::read_text_file(path, encoding);
  require(actual.text == expected.text, "Text file did not round-trip");
  require(actual.encoding == expected.encoding,
          "Text file encoding did not round-trip");
  require(actual.has_byte_order_mark == expected.has_byte_order_mark,
          "Text file byte-order mark did not round-trip");
}

void test_encoding_failure_preserves_file(const QString& directory) {
  const QString path = directory + QStringLiteral("/existing.txt");
  const QByteArray original("existing content");
  QFile output(path);
  require(output.open(QIODevice::WriteOnly), "Could not create test file");
  require(output.write(original) == original.size(),
          "Could not seed test file");
  output.close();

  try {
    jwpqt::qt::write_text_file(
        path, jwpqt::core::TextFile{
                  std::u32string{static_cast<char32_t>(0xd800)},
                  jwpqt::core::TextEncoding::kUtf8, false});
    throw std::runtime_error("Invalid document was saved");
  } catch (const jwpqt::core::Utf8Error&) {
  }

  require(read_bytes(path) == original,
          "Failed save changed the existing file");
  for (const auto encoding : {jwpqt::core::TextEncoding::kUtf16Le,
                              jwpqt::core::TextEncoding::kUtf16Be}) {
    for (const bool bom : {false, true}) {
      test_file_round_trip(directory, encoding, bom);
      try {
        jwpqt::qt::write_text_file(
            path, {std::u32string(1, char32_t{0xd800}), encoding, bom});
        throw std::runtime_error("Invalid UTF-16 document was saved");
      } catch (const jwpqt::core::Utf16Error&) {
      }
      require(read_bytes(path) == original,
              "Failed UTF-16 save changed the existing file");
    }
  }
}

void test_jfc_file_io(const QString& directory) {
  const QString path = directory + QStringLiteral("/cards.jfc");
  QFile output(path);
  const QByteArray old_euc = QByteArray::fromHex("c6fc098e268fabb10a");
  require(output.open(QIODevice::WriteOnly) &&
              output.write(old_euc) == old_euc.size(),
          "Could not create old-EUC JFC fixture");
  output.close();
  const auto file =
      jwpqt::qt::read_text_file(path, jwpqt::core::TextEncoding::kJfc);
  require(file.text == U"\u65e5\t\u00a6\u00e9\n" &&
              file.encoding == jwpqt::core::TextEncoding::kJfc,
          "JFC I/O did not decode old-EUC text");
  jwpqt::qt::write_text_file(path, file);
  const QByteArray canonical = QByteArray::fromHex("e697a509c2a6c3a90a");
  require(read_bytes(path) == canonical,
          "JFC I/O did not replace old EUC with UTF-8");
  try {
    jwpqt::qt::write_text_file(
        path, {std::u32string(1, char32_t{0xd800}),
               jwpqt::core::TextEncoding::kJfc, false});
    throw std::runtime_error("Invalid JFC document was saved");
  } catch (const jwpqt::core::JfcTextError&) {
  }
  require(read_bytes(path) == canonical,
          "Failed JFC save changed the existing file");
}

void test_jwp_file_round_trip(const QString& directory) {
  const QString path = directory + QStringLiteral("/document.jwp");
  jwpqt::core::JwpDocument expected;
  expected.landscape = true;
  expected.summary[0] = {'T', 'i', 't', 'l', 'e'};
  expected.paragraphs = {
      jwpqt::core::JwpParagraph{{'A', 0x467c}, 120, -2, 3, 4, false},
      jwpqt::core::JwpParagraph{{}, 100, 0, 0, 0, true},
  };

  jwpqt::qt::write_jwp_file(path, expected);
  const jwpqt::core::JwpDocument actual = jwpqt::qt::read_jwp_file(path);
  expected.source_version = jwpqt::core::JwpVersion::kJ120;
  require(actual == expected, "JWP document did not round-trip");
}

void test_jwp_encoding_failure_preserves_file(const QString& directory) {
  const QString path = directory + QStringLiteral("/existing.jwp");
  const QByteArray original("existing content");
  QFile output(path);
  require(output.open(QIODevice::WriteOnly), "Could not create JWP test file");
  require(output.write(original) == original.size(),
          "Could not seed JWP test file");
  output.close();

  jwpqt::core::JwpDocument invalid;
  invalid.summary[0] = {0};
  invalid.paragraphs.push_back({});
  try {
    jwpqt::qt::write_jwp_file(path, invalid);
    throw std::runtime_error("Invalid JWP document was saved");
  } catch (const jwpqt::core::JwpFormatError&) {
  }
  require(read_bytes(path) == original,
          "Failed JWP save changed the existing file");
}

void test_read_only_outputs(const QString& directory) {
  using namespace jwpqt;
  const auto fails_read_only = [](auto&& write) {
    try {
      write();
    } catch (const std::exception& error) {
      return std::string_view(error.what()).find("File is read-only") !=
             std::string_view::npos;
    }
    return false;
  };
  const auto make_read_only = [](const QString& path) {
    require(QFile::setPermissions(path, QFileDevice::ReadOwner |
                                            QFileDevice::ReadUser |
                                            QFileDevice::ReadGroup |
                                            QFileDevice::ReadOther),
            "Could not make output read-only");
  };
  const auto make_writable = [](const QString& path) {
    require(QFile::setPermissions(path, QFileDevice::ReadOwner |
                                            QFileDevice::WriteOwner |
                                            QFileDevice::ReadUser |
                                            QFileDevice::WriteUser),
            "Could not restore output permissions");
  };

  const auto text_path = directory + QStringLiteral("/read-only.txt");
  qt::write_text_file(
      text_path, {U"original text", core::TextEncoding::kUtf8, false});
  const auto text_bytes = read_bytes(text_path);
  const auto backup_path = text_path + QStringLiteral("_BAK");
  QFile backup(backup_path);
  require(backup.open(QIODevice::WriteOnly) && backup.write("old backup", 10) == 10,
          "Could not seed read-only backup fixture");
  backup.close();
  make_read_only(text_path);
  require(fails_read_only([&] {
            qt::write_text_file(
                text_path, {U"replacement", core::TextEncoding::kUtf8, false}, true);
          }),
          "Read-only text save was not rejected explicitly");
  require(read_bytes(text_path) == text_bytes,
          "Read-only text save changed the document");
  require(read_bytes(backup_path) == QByteArray("old backup"),
          "Read-only text save changed the backup");
  make_writable(text_path);

  const auto jwp_path = directory + QStringLiteral("/read-only.jce");
  core::JwpDocument document;
  document.paragraphs.push_back({{'A', 0x2422}});
  qt::write_jwp_file(jwp_path, document);
  const auto jwp_bytes = read_bytes(jwp_path);
  make_read_only(jwp_path);
  document.paragraphs.front().text.push_back('B');
  require(fails_read_only([&] { qt::write_jwp_file(jwp_path, document); }) &&
              read_bytes(jwp_path) == jwp_bytes,
          "Read-only JWP save changed the document");
  make_writable(jwp_path);

  const auto project_path = directory + QStringLiteral("/read-only.jpr");
  const core::JwpProject project{"# settings\n", U"/tmp", {U"document.jce"}};
  qt::write_jwp_project_file(project_path, project);
  const auto project_bytes = read_bytes(project_path);
  make_read_only(project_path);
  auto changed_project = project;
  changed_project.paths.push_back(U"second.jce");
  require(fails_read_only([&] {
            qt::write_jwp_project_file(project_path, changed_project);
          }) &&
              read_bytes(project_path) == project_bytes,
          "Read-only project save changed the project");
  make_writable(project_path);
}

void test_jwp_project_file_round_trip(const QString& directory) {
  const QString path = directory + QStringLiteral("/session.jpr");
  const jwpqt::core::JwpProject expected{
      "File.Size = 16\r\n", U"/documents/\u65e5\u672c",
      {U"one.jwp", U"\ufeff\U0001f600.jfc"}};
  jwpqt::qt::write_jwp_project_file(path, expected);
  require(jwpqt::qt::read_jwp_project_file(path) == expected,
          "JWP project did not round-trip through the Qt file boundary");

  const QByteArray original = read_bytes(path);
  jwpqt::core::JwpProject invalid = expected;
  invalid.paths = {std::u32string(U"bad\0path", 8)};
  bool rejected = false;
  try {
    jwpqt::qt::write_jwp_project_file(path, invalid);
  } catch (const jwpqt::core::JwpProjectError&) {
    rejected = true;
  }
  require(rejected && read_bytes(path) == original,
          "Failed JWP project save changed the existing file");
  invalid.paths = {std::u32string(1, char32_t{0xd800})};
  rejected = false;
  try {
    jwpqt::qt::write_jwp_project_file(path, invalid);
  } catch (const jwpqt::core::JwpProjectError&) {
    rejected = true;
  }
  require(rejected && read_bytes(path) == original,
          "Invalid Unicode project path damaged the existing output");

  bool missing_rejected = false;
  try {
    (void)jwpqt::qt::read_jwp_project_file(
        directory + QStringLiteral("/missing.jpr"));
  } catch (const std::runtime_error&) {
    missing_rejected = true;
  }
  require(missing_rejected, "Missing JWP project did not fail explicitly");
}

void test_kanji_info_file_loading(const QString& directory) {
  const QString path = directory + QStringLiteral("/kanjinfo.dat");
  require(!jwpqt::qt::read_kanji_info_file(path).has_value(),
          "Missing kanji information file did not remain optional");

  QByteArray bytes;
  auto append_u16 = [&bytes](quint16 value) {
    bytes.append(static_cast<char>(value & 0xffU));
    bytes.append(static_cast<char>((value >> 8U) & 0xffU));
  };
  auto append_u32 = [&bytes](quint32 value) {
    for (int shift = 0; shift < 32; shift += 8)
      bytes.append(static_cast<char>((value >> shift) & 0xffU));
  };
  append_u32(jwpqt::core::kKanjiInfoMagic);
  append_u32(0U);
  append_u16(1U);
  append_u16(0x3021U);
  bytes.append(12, '\0');
  append_u32(28U << 8U);
  QFile output(path);
  require(output.open(QIODevice::WriteOnly) &&
              output.write(bytes) == bytes.size(),
          "Could not create kanji information fixture");
  output.close();
  const auto loaded = jwpqt::qt::read_kanji_info_file(path);
  require(loaded.has_value() && loaded->count() == 1 &&
              loaded->record(0x3021U).code == 0x3021U,
          "Kanji information file did not load through the Qt boundary");

  output.setFileName(path);
  require(output.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              output.write("bad", 3) == 3,
          "Could not create malformed kanji information fixture");
  output.close();
  bool malformed = false;
  try {
    (void)jwpqt::qt::read_kanji_info_file(path);
  } catch (const jwpqt::core::KanjiInfoError&) {
    malformed = true;
  }
  require(malformed, "Malformed kanji information file was accepted");

  require(QFile::remove(path), "Could not remove kanji information fixture");
  jwpqt::core::KanjiInfoLimits invalid;
  invalid.records = 0;
  bool bad_limits = false;
  try {
    (void)jwpqt::qt::read_kanji_info_file(path, invalid);
  } catch (const jwpqt::core::KanjiInfoError&) {
    bad_limits = true;
  }
  require(bad_limits,
          "Missing kanji information file bypassed limit validation");
}

void test_kanji_lookup_list_loading(const QString& directory) {
  const QString path = directory + QStringLiteral("/radical.dat");
  require(!jwpqt::qt::read_kanji_lookup_lists_file(path, 1).has_value(),
          "Missing kanji lookup list did not remain optional");
  QByteArray bytes;
  bytes.append(char(4));
  bytes.append(char(0));
  bytes.append(char(1));
  bytes.append(char(0));
  bytes.append(char(0x21));
  bytes.append(char(0x30));
  QFile output(path);
  require(output.open(QIODevice::WriteOnly) &&
              output.write(bytes) == bytes.size(),
          "Could not write kanji lookup list fixture");
  output.close();
  const auto loaded =
      jwpqt::qt::read_kanji_lookup_lists_file(path, 1);
  require(loaded.has_value() && loaded->membership_count() == 1 &&
              loaded->group(0) ==
                  std::vector<jwpqt::core::JisCode>{0x3021U},
          "Kanji lookup list did not load through the Qt boundary");
  require(QFile::remove(path), "Could not remove kanji lookup list fixture");
  jwpqt::core::KanjiLookupListLimits invalid;
  invalid.groups = 0;
  bool bad_limits = false;
  try {
    (void)jwpqt::qt::read_kanji_lookup_lists_file(path, 1, invalid);
  } catch (const jwpqt::core::KanjiLookupListError&) {
    bad_limits = true;
  }
  require(bad_limits, "Missing kanji lookup list bypassed limit validation");
}

void test_kanji_color_list_file_round_trip(const QString& directory) {
  const QString path = directory + QStringLiteral("/colkanji.lst");
  require(!jwpqt::qt::read_kanji_color_list_file(path).has_value(),
          "Missing kanji color list did not remain optional");

  jwpqt::core::KanjiColorList list;
  require(list.add(0x5021) && list.add(0x3021),
          "Could not seed kanji color list");
  jwpqt::qt::write_kanji_color_list_file(path, list);
  const auto loaded = jwpqt::qt::read_kanji_color_list_file(path);
  require(loaded.has_value() && loaded->codes() == list.codes(),
          "Kanji color list file did not round-trip");
  require(read_bytes(path) == QByteArray::fromHex("b0a1d0a1"),
          "Kanji color list file was not written canonically");
}

void test_kanji_color_list_file_errors(const QString& directory) {
  const QString malformed = directory + QStringLiteral("/malformed.lst");
  QFile malformed_output(malformed);
  require(malformed_output.open(QIODevice::WriteOnly) &&
              malformed_output.write("\xb0", 1) == 1,
          "Could not seed malformed kanji color list");
  malformed_output.close();
  bool malformed_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_kanji_color_list_file(malformed));
  } catch (const jwpqt::core::KanjiColorListError&) {
    malformed_rejected = true;
  }
  require(malformed_rejected,
          "Malformed kanji color list was accepted by file I/O");

  const QString target = directory + QStringLiteral("/absent-color-list");
  const QString link = directory + QStringLiteral("/dangling-color-list");
  require(QFile::link(target, link),
          "Could not create dangling kanji color list symlink");
  bool dangling_link_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_kanji_color_list_file(link));
  } catch (const std::runtime_error&) {
    dangling_link_rejected = true;
  }
  require(dangling_link_rejected,
          "Dangling kanji color list symlink was treated as missing");

  jwpqt::core::KanjiColorList list;
  list.add(0x3021);
  bool missing_parent_rejected = false;
  try {
    jwpqt::qt::write_kanji_color_list_file(
        directory + QStringLiteral("/missing/colkanji.lst"), list);
  } catch (const std::runtime_error&) {
    missing_parent_rejected = true;
  }
  require(missing_parent_rejected,
          "Kanji color list was written without a parent directory");
}

void test_wnn_preference_file_round_trip(const QString& directory) {
  const QString path = directory + QStringLiteral("/user.sel");
  require(!jwpqt::qt::read_wnn_preferences_file(path, 2).has_value(),
          "Missing WNN preference file did not remain optional");

  jwpqt::core::WnnPreferences preferences(2);
  const jwpqt::core::JwpText input{0x2422};
  const jwpqt::core::WnnLookupResult result{
      {{{0x3021}, 0, false}, {{0x3022}, 2, false}, {input, 4, true}},
      false};
  require(preferences.remember(input, result, 1),
          "Could not seed WNN preference");
  jwpqt::qt::write_wnn_preferences_file(path, preferences);
  require(!preferences.changed(), "Successful save left preferences dirty");

  const auto loaded = jwpqt::qt::read_wnn_preferences_file(path, 2);
  require(loaded.has_value(), "Saved WNN preferences could not be loaded");
  require(loaded->entries() == preferences.entries(),
          "WNN preference file did not round-trip");
  require(read_bytes(path).size() ==
              static_cast<qsizetype>(2 * jwpqt::core::kWnnPreferenceRecordSize),
          "WNN preference file has the wrong wire size");
}

void test_partial_wnn_preference_file(const QString& directory) {
  const QString path = directory + QStringLiteral("/partial.sel");
  QFile output(path);
  require(output.open(QIODevice::WriteOnly),
          "Could not create partial preference file");
  require(output.write("abc", 3) == 3,
          "Could not seed partial preference file");
  output.close();

  const auto loaded = jwpqt::qt::read_wnn_preferences_file(path, 2);
  require(loaded.has_value(), "Partial WNN preferences were not loaded");
  require(loaded->entries()[0].key[0] == 'a' &&
              loaded->entries()[0].key[1] == 'b' &&
              loaded->entries()[0].key[2] == 'c' &&
              loaded->entries()[1] == jwpqt::core::WnnPreferenceEntry{},
          "Partial WNN preferences were not zero-filled");
}

void test_wnn_preference_open_errors(const QString& directory) {
  const QString missing = directory + QStringLiteral("/missing.sel");
  bool invalid_capacity_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_wnn_preferences_file(missing, 0));
  } catch (const jwpqt::core::WnnPreferenceError&) {
    invalid_capacity_rejected = true;
  }
  require(invalid_capacity_rejected,
          "Missing preference file bypassed capacity validation");

  const QString target = directory + QStringLiteral("/absent-target.sel");
  const QString link = directory + QStringLiteral("/dangling.sel");
  require(QFile::link(target, link),
          "Could not create dangling preference symlink");
  bool dangling_link_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_wnn_preferences_file(link, 2));
  } catch (const std::runtime_error&) {
    dangling_link_rejected = true;
  }
  require(dangling_link_rejected,
          "Dangling preference symlink was treated as a missing file");
}

void test_failed_wnn_preference_save_stays_dirty(const QString& directory) {
  const QString path = directory + QStringLiteral("/missing/user.sel");
  jwpqt::core::WnnPreferences preferences(2);
  preferences.resize(3);
  bool rejected = false;
  try {
    jwpqt::qt::write_wnn_preferences_file(path, preferences);
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "WNN preferences were written without a parent");
  require(preferences.changed(),
          "Failed WNN preference save cleared the dirty flag");
}

void test_wnn_user_dictionary_file_round_trip(const QString& directory) {
  const QString path = directory + QStringLiteral("/user.cnv");
  require(!jwpqt::qt::read_wnn_user_dictionary_file(path).has_value(),
          "Missing WNN user dictionary did not remain optional");

  const jwpqt::core::WnnUserDictionary dictionary =
      jwpqt::core::WnnUserDictionary::from_entries({
          {{0x2422}, '*', {{0x3021}, {0x3022}}},
          {{0x2424, 0x246b}, '1', {{0x3023}}},
      });
  jwpqt::qt::write_wnn_user_dictionary_file(path, dictionary);
  const auto loaded = jwpqt::qt::read_wnn_user_dictionary_file(path);
  require(loaded && loaded->entries() == dictionary.entries(),
          "WNN user dictionary file did not round-trip");
  require(jwpqt::qt::read_file_bytes(path) == dictionary.serialize(),
          "WNN user dictionary file was not written canonically");
}

void test_wnn_user_dictionary_file_errors(const QString& directory) {
  const QString malformed = directory + QStringLiteral("/malformed.cnv");
  QFile malformed_file(malformed);
  require(malformed_file.open(QIODevice::WriteOnly) &&
              malformed_file.write("ascii*\xb0\xa1\n", 9) == 9,
          "Could not seed malformed WNN user dictionary");
  malformed_file.close();
  bool malformed_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_wnn_user_dictionary_file(malformed));
  } catch (const jwpqt::core::WnnUserDictionaryError&) {
    malformed_rejected = true;
  }
  require(malformed_rejected,
          "Malformed WNN user dictionary was accepted by file I/O");

  const QString target = directory + QStringLiteral("/absent-user.cnv");
  const QString link = directory + QStringLiteral("/dangling-user.cnv");
  require(QFile::link(target, link),
          "Could not create dangling WNN user dictionary symlink");
  bool dangling_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_wnn_user_dictionary_file(link));
  } catch (const std::runtime_error&) {
    dangling_rejected = true;
  }
  require(dangling_rejected,
          "Dangling WNN user dictionary symlink was treated as missing");

  const QString nondirectory = directory + QStringLiteral("/not-a-directory");
  QFile nondirectory_file(nondirectory);
  require(nondirectory_file.open(QIODevice::WriteOnly),
          "Could not seed non-directory WNN user dictionary parent");
  nondirectory_file.close();
  bool nondirectory_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_wnn_user_dictionary_file(
        nondirectory + QStringLiteral("/user.cnv")));
  } catch (const std::runtime_error&) {
    nondirectory_rejected = true;
  }
  require(nondirectory_rejected,
          "Non-directory WNN user dictionary path was treated as missing");

  bool write_rejected = false;
  try {
    jwpqt::qt::write_wnn_user_dictionary_file(
        directory + QStringLiteral("/missing/user.cnv"),
        jwpqt::core::WnnUserDictionary::from_entries({}));
  } catch (const std::runtime_error&) {
    write_rejected = true;
  }
  require(write_rejected,
          "WNN user dictionary was written without a parent directory");
}

void test_edict_registry_file_round_trip(const QString& directory) {
  using namespace jwpqt::core;

  const QString path = directory + QStringLiteral("/dict.cfg");
  require(!jwpqt::qt::read_edict_registry_file(path).has_value(),
          "Missing EDICT registry did not remain optional");

  EdictRegistry registry;
  registry.entries = {
      {{u'M', u'a', u'i', u'n'},
       {u'd', u'a', u't', u'a', u'/', u'e', u'd', u'i', u'c', u't'},
       EdictRegistryEncoding::kEucJp,
       EdictRegistryNames::kNone,
       EdictRegistrySpecial::kNormal,
       true,
       false,
       true,
       true,
       false},
  };
  jwpqt::qt::write_edict_registry_file(path, registry);
  const auto loaded = jwpqt::qt::read_edict_registry_file(path);
  require(loaded.has_value() && *loaded == registry,
          "EDICT registry file did not round-trip");
  require(jwpqt::qt::read_file_bytes(path) ==
              jwpqt::core::serialize_edict_registry(registry),
          "EDICT registry file was not written canonically");
}

void test_edict_registry_file_errors(const QString& directory) {
  using namespace jwpqt::core;

  EdictRegistryLimits invalid_limits;
  invalid_limits.entries = 0;
  bool invalid_limits_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_edict_registry_file(
        directory + QStringLiteral("/missing-invalid.cfg"), invalid_limits));
  } catch (const EdictRegistryError&) {
    invalid_limits_rejected = true;
  }
  require(invalid_limits_rejected,
          "Missing EDICT registry bypassed configured-limit validation");

  const QString malformed = directory + QStringLiteral("/malformed.cfg");
  QFile malformed_file(malformed);
  require(malformed_file.open(QIODevice::WriteOnly) &&
              malformed_file.write("bad", 3) == 3,
          "Could not seed malformed EDICT registry");
  malformed_file.close();
  bool malformed_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_edict_registry_file(malformed));
  } catch (const EdictRegistryError&) {
    malformed_rejected = true;
  }
  require(malformed_rejected,
          "Malformed EDICT registry was accepted by file I/O");

  const QString target = directory + QStringLiteral("/absent-dict.cfg");
  const QString link = directory + QStringLiteral("/dangling-dict.cfg");
  require(QFile::link(target, link),
          "Could not create dangling EDICT registry symlink");
  bool dangling_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_edict_registry_file(link));
  } catch (const std::runtime_error&) {
    dangling_rejected = true;
  }
  require(dangling_rejected,
          "Dangling EDICT registry symlink was treated as missing");

  EdictRegistry invalid;
  invalid.wire_encoding = static_cast<EdictRegistryWireEncoding>(99);
  const QString existing = directory + QStringLiteral("/existing-dict.cfg");
  QFile existing_file(existing);
  require(existing_file.open(QIODevice::WriteOnly) &&
              existing_file.write("original", 8) == 8,
          "Could not seed existing EDICT registry");
  existing_file.close();
  bool invalid_rejected = false;
  try {
    jwpqt::qt::write_edict_registry_file(existing, invalid);
  } catch (const EdictRegistryError&) {
    invalid_rejected = true;
  }
  require(invalid_rejected && read_bytes(existing) == QByteArray("original"),
          "Failed EDICT registry serialization changed the existing file");

  EdictRegistry empty;
  bool missing_parent_rejected = false;
  try {
    jwpqt::qt::write_edict_registry_file(
        directory + QStringLiteral("/missing/dict.cfg"), empty);
  } catch (const std::runtime_error&) {
    missing_parent_rejected = true;
  }
  require(missing_parent_rejected,
          "EDICT registry was written without a parent directory");
}

void test_edict_user_dictionary_file_round_trip(const QString& directory) {
  using namespace jwpqt::core;

  const QString path = directory + QStringLiteral("/user.dct");
  require(!jwpqt::qt::read_edict_user_dictionary_file(path).has_value(),
          "Missing EDICT user dictionary did not remain optional");

  const EdictUserDictionary dictionary = EdictUserDictionary::from_entries({
      {{0x467c}, {0x242b, 0x244a}, U"caf\u00e9/slash"},
      {{}, {0x2422}, U""},
  });
  jwpqt::qt::write_edict_user_dictionary_file(
      path, dictionary, LegacyCodePage::k1252);
  const auto loaded = jwpqt::qt::read_edict_user_dictionary_file(
      path, LegacyCodePage::k1252);
  require(loaded.has_value() && loaded->entries() == dictionary.entries(),
          "EDICT user dictionary file did not round-trip");
  require(jwpqt::qt::read_file_bytes(path) ==
              dictionary.serialize(LegacyCodePage::k1252),
          "EDICT user dictionary file was not written canonically");
}

void test_edict_user_dictionary_file_errors(const QString& directory) {
  using namespace jwpqt::core;

  const QString missing = directory + QStringLiteral("/missing-user.dct");
  bool invalid_page_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_edict_user_dictionary_file(
        missing, static_cast<LegacyCodePage>(9999)));
  } catch (const EdictUserDictionaryError&) {
    invalid_page_rejected = true;
  }
  require(invalid_page_rejected,
          "Missing EDICT user dictionary bypassed code-page validation");

  const QString malformed = directory + QStringLiteral("/malformed-user.dct");
  QFile malformed_file(malformed);
  require(malformed_file.open(QIODevice::WriteOnly) &&
              malformed_file.write("invalid\n", 8) == 8,
          "Could not seed malformed EDICT user dictionary");
  malformed_file.close();
  bool malformed_rejected = false;
  try {
    static_cast<void>(
        jwpqt::qt::read_edict_user_dictionary_file(malformed));
  } catch (const EdictUserDictionaryError&) {
    malformed_rejected = true;
  }
  require(malformed_rejected,
          "Malformed EDICT user dictionary was accepted by file I/O");

  const QString target = directory + QStringLiteral("/absent-user.dct");
  const QString link = directory + QStringLiteral("/dangling-user.dct");
  require(QFile::link(target, link),
          "Could not create dangling EDICT user dictionary symlink");
  bool dangling_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_edict_user_dictionary_file(link));
  } catch (const std::runtime_error&) {
    dangling_rejected = true;
  }
  require(dangling_rejected,
          "Dangling EDICT user dictionary symlink was treated as missing");

  const QString existing = directory + QStringLiteral("/existing-user.dct");
  QFile existing_file(existing);
  require(existing_file.open(QIODevice::WriteOnly) &&
              existing_file.write("original", 8) == 8,
          "Could not seed existing EDICT user dictionary");
  existing_file.close();
  const EdictUserDictionary unrepresentable =
      EdictUserDictionary::from_entries({{{}, {0x2422}, U"\u20ac"}});
  bool encoding_rejected = false;
  try {
    jwpqt::qt::write_edict_user_dictionary_file(
        existing, unrepresentable, LegacyCodePage::k1251);
  } catch (const EdictUserDictionaryError&) {
    encoding_rejected = true;
  }
  require(encoding_rejected && read_bytes(existing) == QByteArray("original"),
          "Failed EDICT user serialization changed the existing file");

  bool missing_parent_rejected = false;
  try {
    jwpqt::qt::write_edict_user_dictionary_file(
        directory + QStringLiteral("/missing/user.dct"),
        EdictUserDictionary::from_entries({}));
  } catch (const std::runtime_error&) {
    missing_parent_rejected = true;
  }
  require(missing_parent_rejected,
          "EDICT user dictionary was written without a parent directory");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  try {
    test_text_bridge();
    QTemporaryDir directory(QDir::tempPath() +
                            QStringLiteral("/jwpqt-io-test-XXXXXX"));
    require(directory.isValid(), "Could not create temporary test directory");
    test_backups(directory.path());
    test_file_round_trip(directory.path(),
                         jwpqt::core::TextEncoding::kUtf8, true);
    test_file_round_trip(directory.path(),
                         jwpqt::core::TextEncoding::kEucJp, false);
    test_file_round_trip(directory.path(),
                         jwpqt::core::TextEncoding::kShiftJis, false);
    test_encoding_failure_preserves_file(directory.path());
    test_file_round_trip(directory.path(),
                         jwpqt::core::TextEncoding::kJfc, false);
    test_jfc_file_io(directory.path());
    test_jwp_file_round_trip(directory.path());
    test_jwp_encoding_failure_preserves_file(directory.path());
    test_read_only_outputs(directory.path());
    test_jwp_project_file_round_trip(directory.path());
    test_kanji_info_file_loading(directory.path());
    test_kanji_lookup_list_loading(directory.path());
    test_kanji_color_list_file_round_trip(directory.path());
    test_kanji_color_list_file_errors(directory.path());
    test_wnn_preference_file_round_trip(directory.path());
    test_partial_wnn_preference_file(directory.path());
    test_wnn_preference_open_errors(directory.path());
    test_failed_wnn_preference_save_stays_dirty(directory.path());
    test_wnn_user_dictionary_file_round_trip(directory.path());
    test_wnn_user_dictionary_file_errors(directory.path());
    test_edict_registry_file_round_trip(directory.path());
    test_edict_registry_file_errors(directory.path());
    test_edict_user_dictionary_file_round_trip(directory.path());
    test_edict_user_dictionary_file_errors(directory.path());
    std::cout << "All Qt file I/O tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
