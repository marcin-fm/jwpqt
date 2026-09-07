// SPDX-License-Identifier: GPL-2.0-or-later

#include "query_history_io.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>

#include <utility>

#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

QFileInfo validate_path(const QString& path) {
  if (path.isEmpty() || path.size() > 65536 || path.contains(QChar::Null) ||
      to_qstring(from_qstring(path)) != path) {
    throw core::QueryHistoryError("Invalid query history path");
  }
  const QFileInfo info(path);
  if ((info.exists() || info.isSymLink()) && !info.isFile()) {
    throw core::QueryHistoryError("Query history is not a regular file");
  }
  return info;
}

std::optional<std::string> read_bytes(const QString& path) {
  const auto info = validate_path(path);
  if (!info.exists()) return std::nullopt;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    throw core::QueryHistoryError(file.errorString().toStdString());
  }
  constexpr auto limit = static_cast<qint64>(core::kMaximumQueryHistoryFileBytes);
  if (file.size() > limit) {
    throw core::QueryHistoryError("Query history exceeds the file size limit");
  }
  const auto bytes = file.read(limit + 1);
  if (file.error() != QFile::NoError) {
    throw core::QueryHistoryError(file.errorString().toStdString());
  }
  if (bytes.size() > limit || !file.atEnd()) {
    throw core::QueryHistoryError("Query history exceeds the file size limit");
  }
  return bytes.toStdString();
}

}  // namespace

QueryHistorySnapshot read_query_histories(const QString& path) {
  auto source = read_bytes(path);
  auto histories = source ? core::parse_query_history_file(*source)
                          : core::QueryHistories{};
  return {std::move(histories), std::move(source)};
}

std::string write_query_histories(
    const QString& path, const core::QueryHistories& histories,
    const std::optional<std::string>& expected_source) {
  const auto info = validate_path(path);
  auto bytes = core::encode_query_history_file(histories);
  auto identity = info.canonicalFilePath();
  if (identity.isEmpty()) {
    const auto absolute = QDir::isAbsolutePath(path)
        ? path : QDir::currentPath() + QLatin1Char('/') + path;
    const auto slash = absolute.lastIndexOf(QLatin1Char('/'));
    // Resolve the parent through the filesystem, not lexical ".." removal.
    const auto parent = QFileInfo(absolute.left(slash + 1)).canonicalFilePath();
    identity = parent.isEmpty() ? absolute
        : parent + QLatin1Char('/') + absolute.mid(slash + 1);
  }
  QLockFile lock(identity + QStringLiteral(".lock"));
  lock.setStaleLockTime(0);
  if (!lock.tryLock()) {
    throw core::QueryHistoryError("Query history is locked or cannot be locked");
  }
  if (read_query_histories(path).source != expected_source) {
    throw core::QueryHistoryError(
        "Query history changed on disk; reload it before saving");
  }
  QSaveFile file(path);
  const auto size = static_cast<qint64>(bytes.size());
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(bytes.data(), size) != size || !file.commit()) {
    throw core::QueryHistoryError(file.errorString().toStdString());
  }
  return bytes;
}

core::QueryHistories import_legacy_query_histories(
    const QString& path, std::size_t storage_cells, core::LegacyCodePage code_page) {
  const auto bytes = read_bytes(path);
  if (!bytes) throw core::QueryHistoryError("Legacy query history does not exist");
  return core::parse_legacy_query_history_file(*bytes, storage_cells, code_page);
}

}  // namespace jwpqt::qt
