// SPDX-License-Identifier: GPL-2.0-or-later

#include "file_io.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QSaveFile>

namespace jwpqt::qt {
namespace {

std::runtime_error io_error(const char* action, const QString& path,
                            const QString& detail) {
  const QString message =
      QStringLiteral("%1 %2: %3").arg(QString::fromUtf8(action), path, detail);
  return std::runtime_error(message.toUtf8().toStdString());
}

}  // namespace

std::string read_file_bytes(const QString& path) {
  QFile input(path);
  if (!input.open(QIODevice::ReadOnly)) {
    throw io_error("Could not open", path, input.errorString());
  }

  const QByteArray bytes = input.readAll();
  if (input.error() != QFileDevice::NoError) {
    throw io_error("Could not read", path, input.errorString());
  }
  return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

core::TextFile read_text_file(const QString& path,
                              core::TextEncoding encoding) {
  const std::string bytes = read_file_bytes(path);
  return core::decode_text_file(bytes, encoding);
}

void write_text_file(const QString& path, const core::TextFile& file) {
  const std::string bytes = core::encode_text_file(file);
  if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<qint64>::max())) {
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

}  // namespace jwpqt::qt
