// SPDX-License-Identifier: GPL-2.0-or-later
#include "session_io.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>

#include "text_bridge.h"

namespace jwpqt::qt {
namespace {
QFileInfo validate(const QString& path) {
  if (path.isEmpty() || path.size() > 65536 || path.contains(QChar::Null) ||
      to_qstring(from_qstring(path)) != path)
    throw core::JwpProjectError("Invalid session path");
  const QFileInfo info(path);
  if ((info.exists() || info.isSymLink()) && !info.isFile())
    throw core::JwpProjectError("Session is not a regular file");
  return info;
}
}  // namespace

std::optional<std::string> read_session_source(const QString& path) {
  if (!validate(path).exists()) return std::nullopt;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) throw core::JwpProjectError(file.errorString().toStdString());
  const auto limit = static_cast<qint64>(core::JwpProjectLimits{}.encoded_bytes);
  if (file.size() > limit) throw core::JwpProjectError("Session exceeds the file size limit");
  auto bytes = file.read(limit + 1);
  if (file.error() != QFile::NoError || bytes.size() > limit || !file.atEnd())
    throw core::JwpProjectError("Could not read the complete bounded session");
  auto source = bytes.toStdString();
  const auto workspace = decode_project_workspace(core::parse_jwp_project(source), path, {}, {}, false);
  if (workspace.detect_formats) throw core::JwpProjectError("Session requires explicit native file formats");
  return source;
}

std::string write_session_source(const QString& path, const ProjectWorkspace& workspace,
                                const std::optional<std::string>& expected) {
  const auto info = validate(path);
  auto bytes = core::serialize_jwp_project(encode_project_workspace(workspace, false));
  auto identity = info.canonicalFilePath();
  if (identity.isEmpty()) {
    const auto absolute = QDir::isAbsolutePath(path) ? path : QDir::currentPath() + QLatin1Char('/') + path;
    const auto slash = absolute.lastIndexOf(QLatin1Char('/'));
    const auto parent = QFileInfo(absolute.left(slash + 1)).canonicalFilePath();
    identity = parent.isEmpty() ? absolute : parent + QLatin1Char('/') + absolute.mid(slash + 1);
  }
  QLockFile lock(identity + QStringLiteral(".lock"));
  lock.setStaleLockTime(0);
  if (!lock.tryLock()) throw core::JwpProjectError("Session is locked or cannot be locked");
  if (read_session_source(path) != expected)
    throw core::JwpProjectError("Session changed on disk; reload it before saving");
  QSaveFile file(path);
  const auto size = static_cast<qint64>(bytes.size());
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), size) != size || !file.commit())
    throw core::JwpProjectError(file.errorString().toStdString());
  return bytes;
}
}  // namespace jwpqt::qt
