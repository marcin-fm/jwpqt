// SPDX-License-Identifier: GPL-2.0-or-later

#include "file_io.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QSaveFile>

namespace jwpqt::qt {
namespace {

std::runtime_error io_error(const char* action, const QString& path,
                            const QString& detail) {
  const QString message =
      QStringLiteral("%1 %2: %3").arg(QString::fromUtf8(action), path, detail);
  return std::runtime_error(message.toUtf8().toStdString());
}

void write_file_bytes(const QString& path, std::string_view bytes) {
  if (bytes.size() >
      static_cast<std::size_t>(std::numeric_limits<qint64>::max())) {
    throw std::runtime_error("Document is too large to save");
  }

  QSaveFile output(path);
  if (!output.open(QIODevice::WriteOnly)) {
    throw io_error("Could not open", path, output.errorString());
  }

  const qint64 size = static_cast<qint64>(bytes.size());
  if (output.write(bytes.data(), size) != size) {
    output.cancelWriting();
    throw io_error("Could not write", path, output.errorString());
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

}  // namespace

std::string read_file_bytes(const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    throw io_error("Could not open", path, input.errorString());
  }

  return read_open_file_bytes(input, path);
}

core::TextFile read_text_file(const QString& path,
                              core::TextEncoding encoding) {
  const std::string bytes = read_file_bytes(path);
  return core::decode_text_file(bytes, encoding);
}

void write_text_file(const QString& path, const core::TextFile& file) {
  const std::string bytes = core::encode_text_file(file);
  write_file_bytes(path, bytes);
}

core::JwpDocument read_jwp_file(const QString& path) {
  return core::decode_jwp_document(read_file_bytes(path));
}

void write_jwp_file(const QString& path, const core::JwpDocument& document) {
  const std::string bytes = core::encode_jwp_document(document);
  write_file_bytes(path, bytes);
}

std::optional<core::KanjiColorList> read_kanji_color_list_file(
    const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymbolicLink()) {
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

std::optional<core::WnnPreferences> read_wnn_preferences_file(
    const QString& path, std::size_t capacity) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymbolicLink()) {
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

}  // namespace jwpqt::qt
