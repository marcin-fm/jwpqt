// SPDX-License-Identifier: GPL-2.0-or-later

#include "recent_files.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

QByteArray contents(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot read recent-file fixture");
  return file.readAll();
}

void write_bytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              file.write(bytes) == bytes.size(),
          "Cannot write recent-file fixture");
}

template <typename Function>
void rejects(Function function) {
  try {
    function();
  } catch (const jwpqt::qt::RecentFilesError&) {
    return;
  }
  throw std::runtime_error("Invalid recent-file settings were accepted");
}

void test_recent_files() {
  using namespace jwpqt;
  QTemporaryDir directory;
  require(directory.isValid(), "Cannot create recent-file test directory");
  const auto path = directory.filePath(QStringLiteral("recent.json"));
  require(qt::read_recent_documents(path).empty(), "Missing history is not empty");
  const QString unicode_path = directory.filePath(
      QStringLiteral("\u65e5\u672c & \U0001f600.txt"));
  const std::vector<qt::RecentDocument> original{
      {unicode_path, core::TextEncoding::kUtf16Be, core::LegacyCodePage::k1251},
      {directory.filePath(QStringLiteral("../document.jwp")), std::nullopt,
       core::LegacyCodePage::k1258}};
  qt::write_recent_documents(path, original);
  const auto bytes = contents(path);
  const auto loaded = qt::read_recent_documents(path);
  require(loaded.size() == 2 && loaded[0].path == unicode_path &&
              loaded[0].encoding == core::TextEncoding::kUtf16Be &&
              loaded[0].code_page == core::LegacyCodePage::k1251 &&
              loaded[1].path == original[1].path &&
              !loaded[1].encoding &&
              loaded[1].code_page == core::LegacyCodePage::k1258,
          "Recent-file Unicode paths or format metadata did not round-trip");

  for (const auto encoding : {
           core::TextEncoding::kUtf8, core::TextEncoding::kUtf7,
           core::TextEncoding::kUtf16Le, core::TextEncoding::kUtf16Be,
           core::TextEncoding::kJfc, core::TextEncoding::kEucJp,
           core::TextEncoding::kShiftJis, core::TextEncoding::kNewJis,
           core::TextEncoding::kOldJis, core::TextEncoding::kNecJis}) {
    qt::write_recent_documents(path, {{unicode_path, encoding}});
    require(qt::read_recent_documents(path).at(0).encoding == encoding,
            "A recent-file encoding identifier did not round-trip");
  }
  write_bytes(path, bytes);
  const std::vector<std::vector<qt::RecentDocument>> invalid{
      {{QStringLiteral("relative.txt"), core::TextEncoding::kUtf8}},
      {{QStringLiteral("/bad") + QChar::Null, core::TextEncoding::kUtf8}},
      {{QStringLiteral("/bad") + QChar(0xd800), core::TextEncoding::kUtf8}},
      {{unicode_path, std::nullopt, static_cast<core::LegacyCodePage>(1249)}},
      {{unicode_path, std::nullopt}, {unicode_path, core::TextEncoding::kUtf8}},
      std::vector<qt::RecentDocument>(10, original.front()),
      {{QStringLiteral("/") + QString(65536, QLatin1Char('x')), std::nullopt}}};
  for (const auto& documents : invalid) {
    rejects([&] { qt::write_recent_documents(path, documents); });
    require(contents(path) == bytes, "Invalid history replaced the destination");
  }
  std::vector<qt::RecentDocument> oversized;
  for (int i = 0; i < 9; ++i) {
    oversized.push_back({QStringLiteral("/%1/").arg(i) + QString(65500, QChar(1)),
                         std::nullopt});
  }
  rejects([&] { qt::write_recent_documents(path, oversized); });
  require(contents(path) == bytes, "Oversized history replaced the destination");

  const QByteArray record =
      R"({"path":"/a","encoding":"utf-8","code_page":1252})";
  const auto wrap = [](const QByteArray& records) {
    return QByteArray("{\"version\":1,\"files\":[") + records + "]}";
  };
  for (const auto& invalid_json : {
           QByteArray("not json"), QByteArray("[]"), QByteArray("{}"),
           QByteArray("{\"version\":1.5,\"files\":[]}"),
           QByteArray("{\"version\":2,\"files\":[]}"),
           QByteArray("{\"version\":1,\"files\":{},\"extra\":0}"),
           wrap("null"), wrap("{}"), wrap(record + "," + record),
           wrap(QByteArray(record).replace("1252", "1252.5")),
           wrap(QByteArray(record).replace("1252", "true")),
           wrap(QByteArray(record).replace("utf-8", "unknown")),
           wrap(QByteArray(record).replace("/a", "relative")),
           wrap(QByteArray(record).replace("/a", "/\\u0000")),
           wrap(QByteArray(record).replace("/a", "/\\ud800")),
           wrap(QByteArray(record).replace("/a", QByteArray("/\xff", 2))),
           QByteArray(1024 * 1024 + 1, ' ')}) {
    write_bytes(path, invalid_json);
    rejects([&] { qt::read_recent_documents(path); });
    require(contents(path) == invalid_json, "Reading modified invalid history");
  }
  rejects([&] { qt::read_recent_documents(directory.path()); });
  rejects([&] { qt::write_recent_documents(directory.path(), original); });
  const auto dangling = directory.filePath(QStringLiteral("dangling"));
  require(QFile::link(directory.filePath(QStringLiteral("missing")), dangling),
          "Cannot create dangling-link fixture");
  rejects([&] { qt::read_recent_documents(dangling); });

  require(QDir(directory.path()).mkpath(QStringLiteral("actual/nested")),
          "Cannot create symbolic-directory fixture");
  const auto link = directory.filePath(QStringLiteral("link"));
  require(QFile::link(directory.filePath(QStringLiteral("actual/nested")), link),
          "Cannot link the symbolic directory");
  const auto through_link = link + QStringLiteral("/../different.txt");
  const auto lexical = directory.filePath(QStringLiteral("different.txt"));
  write_bytes(through_link, "actual target");
  write_bytes(lexical, "different target");
  qt::write_recent_documents(path, {{through_link, core::TextEncoding::kUtf8},
                                    {lexical, core::TextEncoding::kUtf8}});
  require(contents(qt::read_recent_documents(path).at(0).path) == "actual target",
          "Recent history changed a path across a symbolic directory");
  const auto preserved = contents(path);
  rejects([&] {
    qt::write_recent_documents(path,
        {{through_link, std::nullopt},
         {directory.filePath(QStringLiteral("actual/different.txt")), std::nullopt}});
  });
  require(contents(path) == preserved, "Duplicate aliases replaced history");
  qt::write_recent_documents(path, {});
  require(qt::read_recent_documents(path).empty(), "Clearing history failed");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  try {
    test_recent_files();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
