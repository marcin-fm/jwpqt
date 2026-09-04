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
#include "jwpqt/core/utf8.h"

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

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  try {
    QTemporaryDir directory(QDir::currentPath() +
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
    std::cout << "All Qt file I/O tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
