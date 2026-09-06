// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_LOOKUP_ARTWORK_H
#define JWPQT_QT_LOOKUP_ARTWORK_H

#include <QIcon>
#include <QPalette>
#include <QPixmap>

namespace jwpqt::qt {

QPixmap themed_lookup_artwork(const QPixmap& source, const QPalette& palette);
QIcon themed_lookup_icon(const QPixmap& source, const QPalette& palette);

}  // namespace jwpqt::qt

#endif
