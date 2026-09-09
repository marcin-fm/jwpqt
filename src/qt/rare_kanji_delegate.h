// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QListWidget>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include "jwpqt/core/jis_unicode.h"

namespace jwpqt::qt {

// Decoration only: canonical item data, Copy and Insert remain untouched.
class RareKanjiDelegate final : public QStyledItemDelegate {
 public:
  explicit RareKanjiDelegate(QListWidget* list) : QStyledItemDelegate(list), list_(list) {}
  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    auto size = QStyledItemDelegate::sizeHint(option, index);
    size.rheight() += 4;
    return size;
  }
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    QStyleOptionViewItem text_option(option);
    text_option.rect.adjust(0, 0, 0, -4);
    QStyledItemDelegate::paint(painter, text_option, index);
    if (!enabled()) return;
    QStyleOptionViewItem item(text_option);
    initStyleOption(&item, index);
    auto rect = list_->style()->subElementRect(QStyle::SE_ItemViewItemText, &item, list_);
    QTextLayout layout(item.text, item.font);
    layout.beginLayout();
    auto line = layout.createLine();
    if (!line.isValid()) { layout.endLayout(); return; }
    line.setLineWidth(rect.width());
    layout.endLayout();
    qreal left = rect.left();
    if (item.displayAlignment & Qt::AlignHCenter) left += (rect.width() - line.naturalTextWidth()) / 2;
    else if (item.displayAlignment & Qt::AlignRight) left += rect.width() - line.naturalTextWidth();
    painter->save();
    painter->setClipRect(option.rect);
    const auto color = item.palette.color(item.state & QStyle::State_Selected
        ? QPalette::HighlightedText : QPalette::Text);
    int offset = 0;
    for (auto scalar : item.text.toUcs4()) {
      const auto jis = core::unicode_to_jis_x0208(static_cast<char32_t>(scalar));
      const int length = scalar > 0xffff ? 2 : 1;
      if (jis && *jis >= 0x5000) {
        const qreal x = left + (line.cursorToX(offset) + line.cursorToX(offset + length)) / 2 - 1;
        painter->fillRect(QRectF(x, option.rect.bottom() - 2, 2, 2), color);
      }
      offset += length;
    }
    painter->restore();
  }
 private:
  bool enabled() const {
    for (const QObject* owner = list_; owner; owner = owner->parent()) {
      const auto value = owner->property("jwpqtMarkRareKanji");
      if (value.isValid()) return value.toBool();
    }
    return false;
  }
  QListWidget* list_;
};

inline void install_rare_kanji_marks(QListWidget* list) {
  list->setItemDelegate(new RareKanjiDelegate(list));
}
}  // namespace jwpqt::qt
