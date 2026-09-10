// SPDX-License-Identifier: GPL-2.0-or-later

#include "file_io.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <string>
#include <string_view>
#include <utility>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>

#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

std::runtime_error io_error(const char* action, const QString& path,
                            const QString& detail) {
  const QString message =
      QStringLiteral("%1 %2: %3").arg(QString::fromUtf8(action), path, detail);
  return std::runtime_error(message.toUtf8().toStdString());
}

void write_file_bytes(const QString& path, std::string_view bytes, bool keep_backup = false) {
  if (bytes.size() >
      static_cast<std::size_t>(std::numeric_limits<qint64>::max())) {
    throw std::runtime_error("Document is too large to save");
  }

  const QFileInfo destination(path);
  // WriteUser reflects effective access and may stay true for a privileged
  // process even when the file itself has no write bits.
  const auto writable = QFileDevice::WriteOwner | QFileDevice::WriteGroup |
                        QFileDevice::WriteOther;
  if (destination.isFile() && !(destination.permissions() & writable))
    throw io_error("Could not replace", path, "File is read-only");

  QSaveFile output(path);
  if (!output.open(QIODevice::WriteOnly)) {
    throw io_error("Could not open", path, output.errorString());
  }

  const qint64 size = static_cast<qint64>(bytes.size());
  if (output.write(bytes.data(), size) != size) {
    output.cancelWriting();
    throw io_error("Could not write", path, output.errorString());
  }
  if (keep_backup && (QFileInfo::exists(path) || QFileInfo(path).isSymLink())) {
    const QFileInfo source_info(path);
    const QString backup_path = path + QStringLiteral("_BAK");
    const QFileInfo backup_info(backup_path);
    if (!source_info.isFile() || backup_info.isSymLink() ||
        (backup_info.exists() && !backup_info.isFile()) ||
        source_info.canonicalFilePath() == backup_info.canonicalFilePath())
      throw io_error("Could not back up", path, "Expected a regular source and distinct backup destination");
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) throw io_error("Could not read backup source", path, source.errorString());
    const qint64 original_size = source.size();
    if (original_size < 0) throw io_error("Could not back up", path, "Invalid source size");
    QSaveFile backup(backup_path);
    if (!backup.open(QIODevice::WriteOnly) || !backup.setPermissions(source.permissions()))
      throw io_error("Could not open backup", backup_path, backup.errorString());
    qint64 remaining = original_size;
    while (remaining != 0) {
      const auto chunk = source.read(std::min<qint64>(remaining, 1024 * 1024));
      if (chunk.isEmpty() || source.error() != QFileDevice::NoError ||
          backup.write(chunk) != chunk.size())
        throw io_error("Could not copy backup", backup_path, "Incomplete read or write");
      remaining -= chunk.size();
    }
    if (source.size() != original_size || !source.atEnd())
      throw io_error("Could not back up", path, "Source size changed while copying");
    if (!backup.commit()) throw io_error("Could not replace backup", backup_path, backup.errorString());
  }
  if (!output.commit()) {
    throw io_error("Could not replace", path, output.errorString());
  }
}

std::string read_open_file_bytes(QFile& input, const QString& path) {
  const QByteArray bytes = input.readAll();
  if (input.error() != QFileDevice::NoError) {
    throw io_error("Could not read", path, input.errorString());
  }
  return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

bool path_is_missing(const QString& path) {
  const QByteArray encoded = QFile::encodeName(path);
  const std::filesystem::path native_path(
      std::string(encoded.constData(), static_cast<std::size_t>(encoded.size())));
  std::error_code error;
  const std::filesystem::file_status status =
      std::filesystem::symlink_status(native_path, error);
  return (!error && status.type() == std::filesystem::file_type::not_found) ||
         error == std::errc::no_such_file_or_directory;
}

}  // namespace

std::string read_file_bytes(const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    throw io_error("Could not open", path, input.errorString());
  }

  return read_open_file_bytes(input, path);
}

std::string read_file_bytes(const QString& path, std::size_t maximum_bytes) {
  if (maximum_bytes >= static_cast<std::size_t>(std::numeric_limits<qint64>::max()) ||
      !QFileInfo(path).isFile())
    throw io_error("Could not read", path, "Expected a bounded regular file");
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) throw io_error("Could not open", path, input.errorString());
  const auto bytes = input.read(static_cast<qint64>(maximum_bytes) + 1);
  if (input.error() != QFileDevice::NoError) throw io_error("Could not read", path, input.errorString());
  if (static_cast<std::size_t>(bytes.size()) > maximum_bytes || !input.atEnd())
    throw io_error("Could not read", path, "File exceeds the workspace size limit");
  return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

core::TextFile read_text_file(const QString& path,
                              core::TextEncoding encoding) {
  const std::string bytes = read_file_bytes(path);
  return core::decode_text_file(bytes, encoding);
}

EdictRegistrySnapshot read_edict_registry_snapshot(const QString& path) {
  if (path.isEmpty() || path.size() > 65536 || path.contains(QChar::Null) ||
      to_qstring(from_qstring(path)) != path)
    throw core::EdictRegistryError("Invalid dictionary registry path");
  if (path_is_missing(path)) return {};
  auto bytes = read_file_bytes(path, core::EdictRegistryLimits{}.encoded_bytes);
  auto registry = core::parse_edict_registry(bytes);
  return {std::move(registry), std::move(bytes)};
}

void write_edict_registry_checked(const QString& path, const core::EdictRegistry& registry,
                                 const std::optional<std::string>& expected_source) {
  const auto bytes = core::serialize_edict_registry(registry);
  (void)read_edict_registry_snapshot(path);
  auto identity = QFileInfo(path).canonicalFilePath();
  if (identity.isEmpty()) {
    const auto absolute = QDir::isAbsolutePath(path) ? path : QDir::currentPath() + '/' + path;
    const auto slash = absolute.lastIndexOf('/');
    const auto parent = QFileInfo(absolute.left(slash + 1)).canonicalFilePath();
    identity = parent.isEmpty() ? absolute : parent + '/' + absolute.mid(slash + 1);
  }
  QLockFile lock(identity + QStringLiteral(".lock"));
  lock.setStaleLockTime(0);
  if (!lock.tryLock()) throw core::EdictRegistryError("Dictionary registry is locked or cannot be locked");
  if (read_edict_registry_snapshot(path).source != expected_source)
    throw core::EdictRegistryError("Dictionary registry changed on disk; reopen the manager before saving");
  write_file_bytes(path, bytes);
}

void write_text_file(const QString& path, const core::TextFile& file, bool keep_backup) {
  const std::string bytes = core::encode_text_file(file);
  write_file_bytes(path, bytes, keep_backup);
}

core::JwpDocument read_jwp_file(const QString& path) {
  return core::decode_jwp_document(read_file_bytes(path));
}

void write_jwp_file(const QString& path, const core::JwpDocument& document, bool keep_backup) {
  const std::string bytes = core::encode_jwp_document(document);
  write_file_bytes(path, bytes, keep_backup);
}

core::JwpProject read_jwp_project_file(const QString& path) {
  return core::parse_jwp_project(read_file_bytes(path));
}

void write_jwp_project_file(const QString& path,
                            const core::JwpProject& project) {
  const std::string bytes = core::serialize_jwp_project(project);
  write_file_bytes(path, bytes);
}

std::optional<core::KanjiColorList> read_kanji_color_list_file(
    const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    if (path_is_missing(path)) {
      return std::nullopt;
    }
    throw io_error("Could not open", path, input.errorString());
  }
  return core::KanjiColorList::parse(read_open_file_bytes(input, path));
}

void write_kanji_color_list_file(const QString& path,
                                 const core::KanjiColorList& color_list) {
  const std::string bytes = color_list.serialize();
  write_file_bytes(path, bytes);
}

std::optional<core::KanjiInfoDatabase> read_kanji_info_file(
    const QString& path, const core::KanjiInfoLimits& limits) {
  core::validate_kanji_info_limits(limits);
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    if (path_is_missing(path)) {
      return std::nullopt;
    }
    throw io_error("Could not open", path, input.errorString());
  }
  return core::KanjiInfoDatabase::parse(read_open_file_bytes(input, path),
                                        limits);
}

std::optional<core::KanjiLookupLists> read_kanji_lookup_lists_file(
    const QString& path, std::size_t group_count,
    const core::KanjiLookupListLimits& limits) {
  core::validate_kanji_lookup_list_limits(limits);
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    if (path_is_missing(path)) {
      return std::nullopt;
    }
    throw io_error("Could not open", path, input.errorString());
  }
  return core::KanjiLookupLists::parse(read_open_file_bytes(input, path),
                                       group_count, limits);
}

std::optional<core::WnnPreferences> read_wnn_preferences_file(
    const QString& path, std::size_t capacity) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    if (path_is_missing(path)) {
      static_cast<void>(core::WnnPreferences(capacity));
      return std::nullopt;
    }
    throw io_error("Could not open", path, input.errorString());
  }
  return core::WnnPreferences::parse(read_open_file_bytes(input, path),
                                     capacity);
}

void write_wnn_preferences_file(const QString& path,
                                 core::WnnPreferences& preferences) {
  const std::string bytes = preferences.serialize();
  write_file_bytes(path, bytes);
  preferences.mark_saved();
}

std::optional<core::WnnUserDictionary> read_wnn_user_dictionary_file(
    const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    if (path_is_missing(path)) {
      return std::nullopt;
    }
    throw io_error("Could not open", path, input.errorString());
  }
  return core::WnnUserDictionary::parse(read_open_file_bytes(input, path));
}

void write_wnn_user_dictionary_file(
    const QString& path, const core::WnnUserDictionary& dictionary) {
  const std::string bytes = dictionary.serialize();
  write_file_bytes(path, bytes);
}

std::optional<core::EdictRegistry> read_edict_registry_file(
    const QString& path, const core::EdictRegistryLimits& limits) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    if (path_is_missing(path)) {
      static_cast<void>(
          core::serialize_edict_registry(core::EdictRegistry{}, limits));
      return std::nullopt;
    }
    throw io_error("Could not open", path, input.errorString());
  }
  return core::parse_edict_registry(read_open_file_bytes(input, path), limits);
}

void write_edict_registry_file(const QString& path,
                                const core::EdictRegistry& registry,
                                const core::EdictRegistryLimits& limits) {
  const std::string bytes = core::serialize_edict_registry(registry, limits);
  write_file_bytes(path, bytes);
}

std::optional<core::EdictUserDictionary> read_edict_user_dictionary_file(
    const QString& path, core::LegacyCodePage code_page,
    const core::EdictUserDictionaryLimits& limits) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    if (path_is_missing(path)) {
      static_cast<void>(
          core::EdictUserDictionary::parse({}, code_page, limits));
      return std::nullopt;
    }
    throw io_error("Could not open", path, input.errorString());
  }
  return core::EdictUserDictionary::parse(read_open_file_bytes(input, path),
                                           code_page, limits);
}

void write_edict_user_dictionary_file(
    const QString& path, const core::EdictUserDictionary& dictionary,
    core::LegacyCodePage code_page,
    const core::EdictUserDictionaryLimits& limits) {
  const std::string bytes = dictionary.serialize(code_page, limits);
  write_file_bytes(path, bytes);
}

}  // namespace jwpqt::qt
