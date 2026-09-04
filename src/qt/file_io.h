// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_FILE_IO_H
#define JWPQT_QT_FILE_IO_H

#include <QString>

#include "jwpqt/core/text_file.h"

namespace jwpqt::qt {

core::TextFile read_text_file(const QString& path,
                              core::TextEncoding encoding);
void write_text_file(const QString& path, const core::TextFile& file);

}  // namespace jwpqt::qt

#endif
