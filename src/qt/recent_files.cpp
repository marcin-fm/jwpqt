// SPDX-License-Identifier: GPL-2.0-or-later

#include "recent_files.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

#include <cmath>
#include <utility>

#include "jwpqt/core/utf8.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr qint64 kMaximumJsonBytes = 1024 * 1024;

bool integer_in_range(const QJsonValue& value, int minimum, int maximum) {
  const double number = value.toDouble(minimum - 1);
  return value.isDouble() && number >= minimum && number <= maximum &&
         std::floor(number) == number;
}

QString encoding_id(core::TextEncoding encoding) {
  const auto name = core::text_encoding_name(encoding);
  return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size()))
      .toLower().replace(QLatin1Char(' '), QLatin1Char('-'));
}

QString path_identity(const QString& path) {
  const auto canonical = QFileInfo(path).canonicalFilePath();
  return canonical.isEmpty() ? path : canonical;
}

RecentDocument validate_document(const QString& path, const QString& encoding,
                                 const QJsonValue& code_page) {
  if (path.isEmpty() || path.size() > 65536 ||
      path.contains(QChar::Null) || !path.startsWith(QLatin1Char('/')) ||
      to_qstring(from_qstring(path)) != path) {
    throw RecentFilesError("Recent document path must be valid absolute Unicode");
  }
  std::optional<core::TextEncoding> parsed;
  if (encoding != QStringLiteral("jwp") && encoding != QStringLiteral("jpr")) {
    parsed = core::parse_text_encoding(encoding.toStdString());
    if (!parsed) throw RecentFilesError("Unknown recent document encoding");
  }
  if (!integer_in_range(code_page, 1250, 1258)) {
    throw RecentFilesError("Invalid recent document code page");
  }
  // Lexically removing ".." can change the target across a directory symlink.
  return {path, parsed,
          static_cast<core::LegacyCodePage>(code_page.toInt()), encoding == QStringLiteral("jpr")};
}

}  // namespace

std::vector<RecentDocument> read_recent_documents(const QString& path) {
  if (path.isEmpty()) throw RecentFilesError("Empty recent-file settings path");
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink()) return {};
  if (!info.isFile()) {
    throw RecentFilesError("Recent-file settings are not a regular file");
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    throw RecentFilesError(file.errorString().toStdString());
  }
  if (file.size() > kMaximumJsonBytes) {
    throw RecentFilesError("Recent-file settings exceed the size limit");
  }
  const auto bytes = file.read(kMaximumJsonBytes + 1);
  if (file.error() != QFile::NoError) {
    throw RecentFilesError(file.errorString().toStdString());
  }
  if (bytes.size() > kMaximumJsonBytes || !file.atEnd()) {
    throw RecentFilesError("Recent-file settings exceed the size limit");
  }
  try {
    core::decode_utf8_file(
        std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
  } catch (const core::Utf8Error& error) {
    throw RecentFilesError(error.what());
  }
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !json.isObject()) {
    throw RecentFilesError("Malformed recent-file JSON");
  }
  const auto root = json.object();
  if (root.size() != 2 ||
      !integer_in_range(root.value(QStringLiteral("version")), 1, 1) ||
      !root.value(QStringLiteral("files")).isArray()) {
    throw RecentFilesError("Unsupported recent-file settings schema");
  }
  const auto files = root.value(QStringLiteral("files")).toArray();
  if (files.size() > static_cast<qsizetype>(kMaximumRecentDocuments)) {
    throw RecentFilesError("Too many recent documents");
  }
  std::vector<RecentDocument> documents;
  QSet<QString> paths;
  for (const auto& value : files) {
    const auto object = value.toObject();
    if (!value.isObject() || object.size() != 3 ||
        !object.value(QStringLiteral("path")).isString() ||
        !object.value(QStringLiteral("encoding")).isString()) {
      throw RecentFilesError("Invalid recent document record");
    }
    auto document = validate_document(
        object.value(QStringLiteral("path")).toString(),
        object.value(QStringLiteral("encoding")).toString(),
        object.value(QStringLiteral("code_page")));
    const auto identity = path_identity(document.path);
    if (paths.contains(identity)) {
      throw RecentFilesError("Duplicate recent document path");
    }
    paths.insert(identity);
    documents.push_back(std::move(document));
  }
  return documents;
}

void write_recent_documents(const QString& path,
                            const std::vector<RecentDocument>& documents) {
  if (path.isEmpty()) throw RecentFilesError("Empty recent-file settings path");
  if (documents.size() > kMaximumRecentDocuments) {
    throw RecentFilesError("Too many recent documents");
  }
  QJsonArray files;
  QSet<QString> paths;
  for (const auto& document : documents) {
    if (document.project && document.encoding)
      throw RecentFilesError("A recent project cannot have a text encoding");
    const auto encoding = document.project ? QStringLiteral("jpr")
        : document.encoding ? encoding_id(*document.encoding) : QStringLiteral("jwp");
    const auto validated = validate_document(
        document.path, encoding, static_cast<int>(document.code_page));
    const auto identity = path_identity(validated.path);
    if (paths.contains(identity)) {
      throw RecentFilesError("Duplicate recent document path");
    }
    paths.insert(identity);
    files.append(QJsonObject{
        {QStringLiteral("path"), validated.path},
        {QStringLiteral("encoding"), encoding},
        {QStringLiteral("code_page"), static_cast<int>(validated.code_page)}});
  }
  const auto bytes = QJsonDocument(QJsonObject{
      {QStringLiteral("version"), 1}, {QStringLiteral("files"), files}})
                         .toJson(QJsonDocument::Compact);
  if (bytes.size() > kMaximumJsonBytes) {
    throw RecentFilesError("Recent-file settings exceed the size limit");
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
      !file.commit()) {
    throw RecentFilesError(file.errorString().toStdString());
  }
}

}  // namespace jwpqt::qt
