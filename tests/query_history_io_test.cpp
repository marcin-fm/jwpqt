// SPDX-License-Identifier: GPL-2.0-or-later

#include "query_history_io.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

#include "jwpqt/core/byte_io.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

std::string contents(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Cannot read history fixture");
  return file.readAll().toStdString();
}

void write_bytes(const QString& path, const std::string& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
              file.write(bytes.data(), static_cast<qint64>(bytes.size())) ==
                  static_cast<qint64>(bytes.size()),
          "Cannot write history fixture");
}

template <typename Function>
void rejects(Function function) {
  try {
    function();
  } catch (const jwpqt::core::QueryHistoryError&) {
    return;
  }
  throw std::runtime_error("Invalid or stale query history was accepted");
}

void test_history_io() {
  using namespace jwpqt;
  QTemporaryDir directory(QDir::current().filePath(QStringLiteral("history-io-XXXXXX")));
  require(directory.isValid(), "Cannot create history test directory");
  const auto path = directory.filePath(QStringLiteral("\u65e5\u672c \U0001f600.bin"));
  const auto missing = qt::read_query_histories(path);
  require(!missing.source && missing.histories.dictionary.entries().empty(),
          "Missing history is not optional");
  core::QueryHistories original;
  original.dictionary.remember(U"\ufeff\ufffe\u00a0\U0001f600\t\u611b");
  original.search.remember(U"search");
  original.replace.remember(U"replace");
  const auto bytes = qt::write_query_histories(path, original, missing.source);
  auto loaded = qt::read_query_histories(path);
  require(loaded.source == bytes && contents(path) == bytes &&
              loaded.histories.dictionary.entries() == original.dictionary.entries() &&
              loaded.histories.search.entries() == original.search.entries() &&
              loaded.histories.replace.entries() == original.replace.entries(),
          "History snapshot did not preserve bytes and all Unicode lists");
  rejects([&] { qt::write_query_histories(path, original, missing.source); });
  require(contents(path) == bytes, "A stale absent snapshot replaced a file");

  auto changed = original;
  changed.dictionary.remember(U"new query");
  const auto new_bytes = qt::write_query_histories(path, changed, loaded.source);
  rejects([&] { qt::write_query_histories(path, original, loaded.source); });
  require(contents(path) == new_bytes, "A stale snapshot replaced valid external data");
  require(QFile::remove(path), "Cannot remove history fixture");
  rejects([&] { qt::write_query_histories(path, original, loaded.source); });
  require(!QFileInfo::exists(path), "A stale snapshot recreated a removed history");

  auto invalid = original;
  invalid.replace.set_storage_cells(299);
  rejects([&] { qt::write_query_histories(path, invalid, std::nullopt); });
  require(!QFileInfo::exists(path), "Encoding failure created a history file");
  write_bytes(path, bytes);
  rejects([&] { qt::write_query_histories(path, invalid, bytes); });
  require(contents(path) == bytes, "Encoding failure replaced valid history");
  for (const auto& bad : {std::string{}, std::string("corrupt"), bytes + "x",
                         std::string(core::kMaximumQueryHistoryFileBytes + 1, 'x')}) {
    write_bytes(path, bad);
    rejects([&] { qt::read_query_histories(path); });
    rejects([&] { qt::write_query_histories(path, original, bad); });
    require(contents(path) == bad, "Corrupt history was modified");
  }
  write_bytes(path, bytes);
  for (const auto& bad_path : {QString{}, path + QChar::Null,
                              path + QChar(0xd800), QString(65537, QLatin1Char('x'))}) {
    rejects([&] { qt::read_query_histories(bad_path); });
    rejects([&] { qt::write_query_histories(bad_path, original, std::nullopt); });
  }
  rejects([&] { qt::read_query_histories(directory.path()); });
  rejects([&] { qt::write_query_histories(directory.path(), original, std::nullopt); });
  rejects([&] {
    qt::write_query_histories(directory.filePath(QStringLiteral("missing/history")),
                             original, std::nullopt);
  });
  const auto dangling = directory.filePath(QStringLiteral("dangling"));
  require(QFile::link(directory.filePath(QStringLiteral("absent")), dangling),
          "Cannot create dangling history link");
  rejects([&] { qt::read_query_histories(dangling); });
  rejects([&] { qt::write_query_histories(dangling, original, std::nullopt); });
  require(QFileInfo(dangling).isSymLink(), "Dangling link was replaced");
#ifdef Q_OS_UNIX
  const auto fifo = directory.filePath(QStringLiteral("fifo"));
  require(::mkfifo(QFile::encodeName(fifo).constData(), 0600) == 0,
          "Cannot create nonregular history fixture");
  rejects([&] { qt::read_query_histories(fifo); });
  rejects([&] { qt::write_query_histories(fifo, original, std::nullopt); });
#endif

  QLockFile lock(path + QStringLiteral(".lock"));
  require(lock.tryLock(), "Cannot lock history fixture");
  rejects([&] { qt::write_query_histories(path, changed, bytes); });
  require(contents(path) == bytes, "Locked history was replaced");
  lock.unlock();
  write_bytes(path + QStringLiteral(".lock"), "not a lock file");
  rejects([&] { qt::write_query_histories(path, changed, bytes); });
  require(contents(path + QStringLiteral(".lock")) == "not a lock file",
          "Unrecognized lock file was removed");
  require(QFile::remove(path + QStringLiteral(".lock")), "Cannot remove test lock");

  const auto alias = directory.filePath(QStringLiteral("alias"));
  require(QFile::link(path, alias), "Cannot create history alias");
  require(lock.tryLock(), "Cannot relock history fixture");
  rejects([&] { qt::write_query_histories(alias, changed, bytes); });
  lock.unlock();
  require(qt::write_query_histories(alias, changed, bytes) == new_bytes &&
              QFileInfo(alias).isSymLink() && contents(path) == new_bytes,
          "Alias write did not update its real target atomically");

  require(QDir(directory.path()).mkpath(QStringLiteral("actual/nested")),
          "Cannot create symbolic parent fixture");
  const auto link = directory.filePath(QStringLiteral("link"));
  require(QFile::link(directory.filePath(QStringLiteral("actual/nested")), link),
          "Cannot create symbolic parent");
  const auto symbolic_path = link + QStringLiteral("/../history.bin");
  const auto real_path = directory.filePath(QStringLiteral("actual/history.bin"));
  const auto lexical_path = directory.filePath(QStringLiteral("history.bin"));
  write_bytes(lexical_path, "unrelated lexical target");
  QLockFile parent_lock(real_path + QStringLiteral(".lock"));
  require(parent_lock.tryLock(), "Cannot lock missing symbolic target");
  rejects([&] { qt::write_query_histories(symbolic_path, original, std::nullopt); });
  parent_lock.unlock();
  qt::write_query_histories(symbolic_path, original, std::nullopt);
  require(contents(real_path) == bytes && contents(lexical_path) == "unrelated lexical target",
          "History path was cleaned across a symbolic directory");

  const auto legacy_path = directory.filePath(QStringLiteral("JWPxp.his"));
  core::ByteWriter legacy;
  legacy.write_u32_le(core::kLegacyQueryHistoryMagic);
  for (int kind = 0; kind < 3; ++kind) {
    legacy.write_u32_le(1);
    for (int i = 0; i < 10; ++i)
      legacy.write_u16_le(i == 0 ? 0 : i == 1 ? 1 : i == 3 ? 0x80 : 0xffff);
  }
  legacy.write_bytes("opaque recent paths");
  const auto legacy_bytes = legacy.take_bytes();
  write_bytes(legacy_path, legacy_bytes);
  const auto imported = qt::import_legacy_query_histories(
      legacy_path, 10, core::LegacyCodePage::k1251);
  require(imported.dictionary.entries().front() == U"\u0402" &&
              imported.search.entries().front() == U"\u0402" &&
              imported.replace.entries().front() == U"\u0402" &&
              contents(legacy_path) == legacy_bytes,
          "Legacy import changed its source or omitted history kinds");
  rejects([&] {
    qt::import_legacy_query_histories(legacy_path, 10, core::LegacyCodePage::k1252);
  });
  rejects([&] {
    qt::import_legacy_query_histories(dangling, 10, core::LegacyCodePage::k1251);
  });
  rejects([&] {
    qt::import_legacy_query_histories(directory.filePath(QStringLiteral("absent")),
                                     10, core::LegacyCodePage::k1251);
  });
  rejects([&] { qt::read_query_histories(legacy_path); });
  rejects([&] { qt::write_query_histories(legacy_path, original, legacy_bytes); });
  require(contents(legacy_path) == legacy_bytes, "Native storage overwrote legacy history");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  try {
    test_history_io();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
