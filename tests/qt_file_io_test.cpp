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

#include "file_io.h"
#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/utf8.h"
#include "jwpqt/core/wnn_preferences.h"
#include "jwpqt/core/wnn_user_dictionary.h"

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

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  try {
    QTemporaryDir directory(QDir::tempPath() +
                            QStringLiteral("/jwpqt-io-test-XXXXXX"));
    require(directory.isValid(), "Could not create temporary test directory");
    test_file_round_trip(directory.path(),
                         jwpqt::core::TextEncoding::kUtf8, true);
    test_file_round_trip(directory.path(),
                         jwpqt::core::TextEncoding::kEucJp, false);
    test_file_round_trip(directory.path(),
                         jwpqt::core::TextEncoding::kShiftJis, false);
    test_encoding_failure_preserves_file(directory.path());
    test_jwp_file_round_trip(directory.path());
    test_jwp_encoding_failure_preserves_file(directory.path());
    test_kanji_color_list_file_round_trip(directory.path());
    test_kanji_color_list_file_errors(directory.path());
    test_wnn_preference_file_round_trip(directory.path());
    test_partial_wnn_preference_file(directory.path());
    test_wnn_preference_open_errors(directory.path());
    test_failed_wnn_preference_save_stays_dirty(directory.path());
    test_wnn_user_dictionary_file_round_trip(directory.path());
    test_wnn_user_dictionary_file_errors(directory.path());
    std::cout << "All Qt file I/O tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
