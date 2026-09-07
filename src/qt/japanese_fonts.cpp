// SPDX-License-Identifier: GPL-2.0-or-later

#include "japanese_fonts.h"

#include <QFont>
#include <QFontDatabase>
#include <QStyle>
#include <QVariant>
#include <QWidget>

namespace jwpqt::qt {
namespace {

constexpr char kRole[] = "_jwpqt_font_role";
constexpr char kOriginal[] = "_jwpqt_font_original";
constexpr char kStrip[] = "_jwpqt_font_strip";
constexpr char kSettings[] = "_jwpqt_font_settings";

}  // namespace

void assign_japanese_font(QWidget& widget, JapaneseFontRole role, bool horizontal_strip) {
  const int index = static_cast<int>(role);
  if (index < 0 || index >= static_cast<int>(JapaneseFontRole::kCount)) return;
  if (!widget.property(kOriginal).isValid()) widget.setProperty(kOriginal, QVariant::fromValue(widget.font()));
  widget.setProperty(kRole, index);
  widget.setProperty(kStrip, horizontal_strip);
  QFont font = widget.property(kOriginal).value<QFont>();
  for (QWidget* owner = &widget; owner; owner = owner->parentWidget()) {
    const auto settings = owner->property(kSettings).toList();
    if (settings.size() != static_cast<int>(JapaneseFontRole::kCount)) continue;
    const auto setting = settings[index].toList();
    if (setting.size() != 2) continue;
    if (!setting[0].toString().isEmpty()) font.setFamily(setting[0].toString());
    if (role != JapaneseFontRole::kBig) font.setPixelSize(setting[1].toInt());
    break;
  }
  widget.setFont(font);
  if (horizontal_strip) {
    widget.setFixedHeight(widget.fontMetrics().height() + 12 +
                          widget.style()->pixelMetric(QStyle::PM_ScrollBarExtent));
  }
}

QStringList set_japanese_fonts(QWidget& owner, const ApplicationSettings& settings) {
  QVariantList resolved;
  QStringList warnings;
  const auto families = QFontDatabase::families();
  constexpr std::size_t system = static_cast<std::size_t>(JapaneseFontRole::kSystem);
  constexpr std::size_t edit = static_cast<std::size_t>(JapaneseFontRole::kEdit);
  constexpr std::size_t file = static_cast<std::size_t>(JapaneseFontRole::kFile);
  for (std::size_t role = 0; role < settings.fonts.size(); ++role) {
    std::size_t base = role;
    while (base != system && settings.fonts[base].automatic) {
      if (base == static_cast<std::size_t>(JapaneseFontRole::kList) ||
          base == static_cast<std::size_t>(JapaneseFontRole::kKanjiBar)) base = edit;
      else if (base == static_cast<std::size_t>(JapaneseFontRole::kBig)) base = file;
      else base = system;
    }
    QString family = settings.fonts[base].family;
    if (!family.isEmpty() && !families.contains(family, Qt::CaseInsensitive)) {
      const auto warning = QObject::tr("Font '%1' is unavailable; a native fallback is used.").arg(family);
      if (!warnings.contains(warning)) warnings.push_back(warning);
      family.clear();
    }
    const int size = role == static_cast<std::size_t>(JapaneseFontRole::kTable)
                         ? 16 : settings.fonts[base].size;
    resolved.push_back(QVariantList{family, size});
  }
  owner.setProperty(kSettings, resolved);
  for (auto* widget : owner.findChildren<QWidget*>()) {
    if (!widget->property(kRole).isValid()) continue;
    assign_japanese_font(*widget, static_cast<JapaneseFontRole>(widget->property(kRole).toInt()),
                         widget->property(kStrip).toBool());
  }
  return warnings;
}

}  // namespace jwpqt::qt
