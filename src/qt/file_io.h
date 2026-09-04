// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_FILE_IO_H
#define JWPQT_QT_FILE_IO_H

#include <QString>

#include "jwpqt/core/utf8.h"

namespace jwpqt::qt {

core::Utf8File read_utf8_file(const QString& path);
void write_utf8_file(const QString& path, const core::Utf8File& file);

}  // namespace jwpqt::qt

#endif
