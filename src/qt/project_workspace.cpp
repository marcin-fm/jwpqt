// SPDX-License-Identifier: GPL-2.0-or-later

#include "project_workspace.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include "jwpqt/core/jwp_configuration.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr std::string_view kMarker = "# jwpqt-project ";

std::string metadata_lines(std::string_view text, std::optional<std::string>& metadata) {
  core::parse_jwp_configuration(text);
  std::string retained;
  for (std::size_t begin = 0; begin < text.size();) {
    auto end = text.find_first_of("\r\n", begin);
    if (end == std::string_view::npos) end = text.size();
    auto next = end;
    if (next < text.size() && text[next] == '\r') ++next;
    if (next < text.size() && text[next] == '\n') ++next;
    auto line = text.substr(begin, end - begin);
    const auto first = line.find_first_not_of(" \t");
    if (first != std::string_view::npos) line.remove_prefix(first);
    if (line.substr(0, kMarker.size()) == kMarker) {
      if (metadata) throw core::JwpProjectError("Duplicate native project metadata");
      metadata = std::string(line.substr(kMarker.size()));
    } else {
      retained.append(text.substr(begin, next - begin));
    }
    begin = next;
  }
  return retained;
}

int number(const QJsonValue& value, int minimum, int maximum) {
  if (!value.isDouble() || value.toDouble() < minimum ||
      value.toDouble() > maximum || value.toDouble() != value.toInt())
    throw core::JwpProjectError("Invalid native project numeric value");
  return value.toInt();
}

bool windows_absolute(const QString& path) {
  const auto first = path.isEmpty() ? QChar{} : path[0].toLower();
  return (path.size() >= 3 && first >= QLatin1Char('a') && first <= QLatin1Char('z') && path[1] == QLatin1Char(':') &&
          (path[2] == QLatin1Char('/') || path[2] == QLatin1Char('\\'))) ||
         path.startsWith(QStringLiteral("\\\\")) || path.startsWith(QStringLiteral("//"));
}

QString clean_windows_path(QString path) {
  path.replace(QLatin1Char('\\'), QLatin1Char('/'));
  const bool unc = path.startsWith(QStringLiteral("//"));
  path = QDir::cleanPath(path);
  if (unc && !path.startsWith(QStringLiteral("//"))) path.prepend(QLatin1Char('/'));
  return path;
}

bool below(const QString& path, const QString& directory, Qt::CaseSensitivity sensitivity) {
  return path.compare(directory, sensitivity) == 0 ||
         path.startsWith(directory + (directory.endsWith(QLatin1Char('/'))
             ? QString{} : QStringLiteral("/")), sensitivity);
}

std::u32string scalar_path(const QString& path) {
  if (path.isEmpty() || path.contains(QChar(0)) ||
      static_cast<std::size_t>(path.size()) > core::JwpProjectLimits{}.path_bytes / 2)
    throw core::JwpProjectError("Invalid or oversized project path");
  auto text = from_qstring(path);
  if (to_qstring(text) != path) throw core::JwpProjectError("Invalid Unicode project path");
  return text;
}

}  // namespace

ProjectPathError::ProjectPathError(QString source_directory)
    : core::JwpProjectError((QStringLiteral("Choose a Linux directory for ") + source_directory)
                               .toUtf8().toStdString()),
      source_directory_(std::move(source_directory)) {}

ProjectWorkspace decode_project_workspace(const core::JwpProject& project,
    const QString& project_path, const ApplicationSettings& base,
    const std::vector<ProjectPathMapping>& mappings) {
  if (project.paths.size() > kMaximumWorkspaceDocuments ||
      mappings.size() > kMaximumWorkspaceDocuments || project_path.isEmpty())
    throw core::JwpProjectError("Invalid project location or workspace size");
  (void)scalar_path(project_path);
  ProjectWorkspace result;
  result.settings = read_application_settings(project.configuration, base);
  std::optional<std::string> metadata;
  (void)metadata_lines(project.configuration, metadata);
  QJsonArray formats;
  std::size_t first = 0;
  if (metadata) {
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(QByteArray::fromStdString(*metadata), &error);
    const auto object = json.object();
    if (error.error != QJsonParseError::NoError || !json.isObject() || object.size() != 4 ||
        number(object.value("version"), 1, 1) != 1 ||
        object.value("paths").toString() != QStringLiteral("posix") ||
        !object.value("formats").isArray())
      throw core::JwpProjectError("Invalid or unsupported native project metadata");
    first = static_cast<std::size_t>(number(object.value("first"), 0,
        project.paths.empty() ? 0 : static_cast<int>(project.paths.size() - 1)));
    formats = object.value("formats").toArray();
    if (static_cast<std::size_t>(formats.size()) != project.paths.size())
      throw core::JwpProjectError("Project formats do not match the file list");
    result.detect_formats = false;
  }

  QSet<QString> mapping_keys;
  for (const auto& mapping : mappings) {
    (void)scalar_path(mapping.source_directory);
    (void)scalar_path(mapping.local_directory);
    if (!QDir::isAbsolutePath(mapping.local_directory) || !QFileInfo(mapping.local_directory).isDir())
      throw core::JwpProjectError("A project mapping must name an existing absolute Linux directory");
    if (!QDir::isAbsolutePath(mapping.source_directory) && !windows_absolute(mapping.source_directory))
      throw core::JwpProjectError("A project mapping needs an absolute source directory");
    const bool windows = result.detect_formats && windows_absolute(mapping.source_directory);
    QString key = windows ? clean_windows_path(mapping.source_directory).toCaseFolded() : mapping.source_directory;
    while (key.size() > 1 && key.endsWith(QLatin1Char('/'))) key.chop(1);
    key.prepend(windows ? QStringLiteral("windows:") : QStringLiteral("posix:"));
    if (mapping_keys.contains(key)) throw core::JwpProjectError("Duplicate project directory mappings");
    mapping_keys.insert(key);
  }
  QString location = QFileInfo(project_path).canonicalFilePath();
  if (location.isEmpty()) location = QDir::isAbsolutePath(project_path)
      ? project_path : QDir::currentPath() + QStringLiteral("/") + project_path;
  const QString parent = location.left(std::max<qsizetype>(1, location.lastIndexOf(QLatin1Char('/'))));
  const QString directory = to_qstring(project.current_directory);
  if (!directory.isEmpty()) (void)scalar_path(directory);
  QSet<QString> identities;
  for (std::size_t i = 0; i < project.paths.size(); ++i) {
    ProjectDocument entry;
    QString path = to_qstring(project.paths[i]);
    (void)scalar_path(path);
    const bool windows = result.detect_formats &&
        (windows_absolute(directory) || directory.contains(QLatin1Char('\\')) ||
         windows_absolute(path) || path.contains(QLatin1Char('\\')));
    QString source_directory = directory;
    if (windows) {
      source_directory.replace(QLatin1Char('\\'), QLatin1Char('/'));
      path.replace(QLatin1Char('\\'), QLatin1Char('/'));
      if (path.size() >= 2 && path[1] == QLatin1Char(':') && !windows_absolute(path))
        throw core::JwpProjectError("Drive-relative Windows project paths are not resolvable");
      if (!windows_absolute(path)) {
        if (path.startsWith(QLatin1Char('/')) && source_directory.size() >= 2 &&
            source_directory[1] == QLatin1Char(':')) path = source_directory.left(2) + path;
        else path = source_directory + QStringLiteral("/") + path;
      }
      // Only Windows source paths use lexical parent resolution; never clean Linux symlink paths.
      path = clean_windows_path(path);
      source_directory = clean_windows_path(source_directory);
      if (!windows_absolute(path))
        throw core::JwpProjectError("Windows project paths need an absolute stored directory");
    } else {
      if (!QDir::isAbsolutePath(source_directory))
        source_directory = parent + (source_directory.isEmpty() ? QString{} : QStringLiteral("/") + source_directory);
      if (!QDir::isAbsolutePath(path)) path = source_directory + QStringLiteral("/") + path;
    }
    const auto sensitivity = windows ? Qt::CaseInsensitive : Qt::CaseSensitive;
    QString mapped;
    qsizetype longest = -1;
    for (const auto& mapping : mappings) {
      QString source = mapping.source_directory;
      if (windows) source = clean_windows_path(source);
      while (source.size() > 1 && source.endsWith(QLatin1Char('/'))) source.chop(1);
      if (!below(path, source, sensitivity)) continue;
      if (source.size() == longest)
        throw core::JwpProjectError("Ambiguous project directory mappings");
      if (source.size() > longest) {
        longest = source.size();
        QString suffix = path.mid(source.size());
        if (!suffix.isEmpty() && !suffix.startsWith(QLatin1Char('/'))) suffix.prepend(QLatin1Char('/'));
        mapped = mapping.local_directory + suffix;
      }
    }
    if (!mapped.isEmpty()) path = mapped;
    else if (windows) {
      const QString missing = below(path, source_directory, sensitivity) ? source_directory
          : path.left(path.lastIndexOf(QLatin1Char('/')) + 1);
      throw ProjectPathError(missing);
    }
    entry.path = path;
    (void)scalar_path(entry.path);
    const auto canonical = QFileInfo(path).canonicalFilePath();
    const auto identity = canonical.isEmpty() ? path : canonical;
    if (identities.contains(identity)) throw core::JwpProjectError("A project refers to the same document more than once");
    identities.insert(identity);
    entry.code_page = static_cast<core::LegacyCodePage>(result.settings.translation_code_page == 0
        ? 1252 : result.settings.translation_code_page);
    if (!result.detect_formats) {
      const auto object = formats.at(static_cast<qsizetype>(i)).toObject();
      if (object.size() != 3 || !object.value("encoding").isString() ||
          !object.value("japanese").isBool())
        throw core::JwpProjectError("Invalid project document format");
      const auto name = object.value("encoding").toString().toStdString();
      if (name != "jwp") {
        entry.encoding = core::parse_text_encoding(name);
        if (!entry.encoding) throw core::JwpProjectError("Unsupported project document encoding");
      }
      entry.code_page = static_cast<core::LegacyCodePage>(number(object.value("code_page"), 1250, 1258));
      entry.japanese_editing = object.value("japanese").toBool();
    }
    result.documents.push_back(std::move(entry));
  }
  if (!result.documents.empty()) {
    std::rotate(result.documents.begin(), result.documents.begin() + first, result.documents.end());
    result.current_document = (result.documents.size() - 1 + result.documents.size() - first) % result.documents.size();
  }
  return result;
}

core::JwpProject encode_project_workspace(const ProjectWorkspace& workspace) {
  const auto count = workspace.documents.size();
  if (workspace.detect_formats || count > kMaximumWorkspaceDocuments ||
      (count == 0 ? workspace.current_document != 0 : workspace.current_document >= count))
    throw core::JwpProjectError("Cannot save an invalid or undetected workspace");
  core::JwpProject result;
  std::optional<std::string> previous;
  result.configuration = metadata_lines(write_application_settings(workspace.settings), previous);
  result.current_directory = scalar_path(QDir::currentPath());
  QJsonArray formats;
  QSet<QString> identities;
  for (std::size_t i = 0; i < count; ++i) {
    const auto& entry = workspace.documents[(workspace.current_document + 1 + i) % count];
    if (!QDir::isAbsolutePath(entry.path)) throw core::JwpProjectError("Save project documents with absolute paths");
    result.paths.push_back(scalar_path(entry.path));
    const auto canonical = QFileInfo(entry.path).canonicalFilePath();
    const auto identity = canonical.isEmpty() ? entry.path : canonical;
    if (identities.contains(identity)) throw core::JwpProjectError("Duplicate project document path");
    identities.insert(identity);
    const int code_page = static_cast<int>(entry.code_page);
    if (code_page < 1250 || code_page > 1258) throw core::JwpProjectError("Invalid project code page");
    const auto name = entry.encoding
        ? QString::fromLatin1(core::text_encoding_name(*entry.encoding)).toLower().replace(QLatin1Char(' '), QLatin1Char('-'))
        : QStringLiteral("jwp");
    if (entry.encoding && core::parse_text_encoding(name.toStdString()) != entry.encoding)
      throw core::JwpProjectError("Invalid project text encoding");
    formats.append(QJsonObject{{"encoding", name}, {"code_page", code_page}, {"japanese", entry.japanese_editing}});
  }
  const int first = count == 0 ? 0 : static_cast<int>((count - workspace.current_document - 1) % count);
  const QJsonObject metadata{{"version", 1}, {"paths", QStringLiteral("posix")}, {"first", first}, {"formats", formats}};
  if (!result.configuration.empty() && result.configuration.back() != '\r' && result.configuration.back() != '\n')
    result.configuration += "\r\n";
  result.configuration += std::string(kMarker) + QJsonDocument(metadata).toJson(QJsonDocument::Compact).toStdString() + "\r\n";
  core::parse_jwp_configuration(result.configuration);
  return result;
}

}  // namespace jwpqt::qt
