// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>

#include <QApplication>
#include <QCheckBox>
#include <QEventLoop>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

#include "jwpqt/core/kanji_lookup_lists.h"
#include "kanji_lookup_dialog.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void append_u16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void append_u32(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
}

void put_u16(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset] = static_cast<char>(value & 0xffU);
  bytes[offset + 1] = static_cast<char>((value >> 8U) & 0xffU);
}

void put_u32(std::string& bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes[offset + shift / 8] = static_cast<char>((value >> shift) & 0xffU);
}

jwpqt::core::KanjiLookupLists lists(
    const std::vector<std::vector<jwpqt::core::JisCode>>& groups) {
  std::string bytes(groups.size() * 4U, '\0');
  std::size_t offset = bytes.size();
  for (std::size_t i = 0; i < groups.size(); ++i) {
    put_u16(bytes, i * 4U, static_cast<std::uint16_t>(offset));
    put_u16(bytes, i * 4U + 2U,
            static_cast<std::uint16_t>(groups[i].size()));
    for (const auto code : groups[i]) append_u16(bytes, code);
    offset = bytes.size();
  }
  return jwpqt::core::KanjiLookupLists::parse(bytes, groups.size());
}

jwpqt::core::KanjiInfoDatabase information() {
  constexpr std::size_t count = 3;
  constexpr std::size_t variable = 12 + count * 16;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0U);
  append_u16(bytes, static_cast<std::uint16_t>(count));
  append_u16(bytes, 0x3023U);
  bytes.resize(variable, '\0');
  for (std::size_t i = 0; i < count; ++i) {
    put_u16(bytes, 12U + i * 16U,
            static_cast<std::uint16_t>((i + 1U) << 8U));
    put_u32(bytes, 12U + i * 16U + 12U,
            static_cast<std::uint32_t>(variable) << 8U);
  }
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

void test_dialog() {
  const auto radicals = lists({{0x3021U, 0x3022U}, {0x3022U}});
  std::vector<std::vector<jwpqt::core::JisCode>> stroke_values(30);
  stroke_values[0] = {0x3021U};
  stroke_values[1] = {0x3022U};
  const auto strokes = lists(stroke_values);
  const auto info = information();
  std::vector<jwpqt::core::JisCode> inserted;
  jwpqt::core::JisCode shown = 0;
  jwpqt::qt::KanjiLookupDialog dialog(
      radicals, strokes, info, QPixmap{},
      [&](const auto& codes) { inserted = codes; },
      [&](jwpqt::core::JisCode code) { shown = code; });
  dialog.set_selected_radicals({0, 1});
  require(dialog.search() &&
              dialog.result_codes() ==
                  std::vector<jwpqt::core::JisCode>{0x3022U},
          "Native radical dialog did not show intersection results");
  try {
    dialog.set_selected_radicals({2});
    require(false, "Out-of-range native radical selection was accepted");
  } catch (const jwpqt::core::KanjiLookupListError&) {
  }
  require(dialog.selected_radicals() == std::vector<std::size_t>{0, 1},
          "Rejected native radical selection changed working state");
  auto* list =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiLookupResults"));
  require(list != nullptr && list->count() == 1,
          "Native radical dialog result list is unavailable");
  require(list->flow() == QListView::LeftToRight && !list->isWrapping() &&
              list->font().pixelSize() == 16 && list->item(0)->isSelected(),
          "Radical results are not a readable selected character strip");
  list->item(0)->setSelected(true);
  auto* insert =
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiLookupInsert"));
  auto* info_button =
      dialog.findChild<QPushButton*>(QStringLiteral("kanjiLookupInfo"));
  require(insert != nullptr && info_button != nullptr,
          "Native radical result actions are unavailable");
  insert->click();
  info_button->click();
  require(inserted == std::vector<jwpqt::core::JisCode>{0x3022U} &&
              shown == 0x3022U,
          "Native radical dialog callbacks received wrong results");
  auto* automatic = dialog.findChild<QCheckBox*>(QStringLiteral("kanjiLookupAutoSearch"));
  auto* timer = dialog.findChild<QTimer*>(QStringLiteral("kanjiLookupSearchTimer"));
  auto* clear = dialog.findChild<QPushButton*>(QStringLiteral("kanjiLookupClear"));
  require(automatic && automatic->isChecked() && timer && clear,
          "Radical lookup Clear/Auto Search controls are missing");
  dialog.show();
  QApplication::processEvents();
  auto* selected = dialog.findChild<QToolButton*>(QStringLiteral("radicalButton1"));
  QPalette palette = dialog.palette();
  palette.setColor(QPalette::Highlight, QColor(48, 140, 198));
  dialog.setPalette(palette);
  QApplication::processEvents();
  const QImage selected_image = selected->grab().toImage();
  const qreal scale = selected_image.devicePixelRatio();
  require(selected_image.pixelColor(qRound(scale), selected_image.height() / 2) ==
              palette.color(QPalette::Highlight),
          "Selected radical has no visible highlight border");
  timer->setInterval(1);
  dialog.set_selected_radicals({0});
  QEventLoop loop;
  QTimer::singleShot(20, &loop, &QEventLoop::quit);
  loop.exec();
  require(dialog.result_codes().size() == 2, "Selecting radicals did not run automatic search");
  dialog.set_selected_radicals({1});
  clear->click();
  QTimer::singleShot(20, &loop, &QEventLoop::quit);
  loop.exec();
  require(dialog.selected_radicals().empty() && dialog.result_codes().empty() && !timer->isActive(),
          "Radical Clear left selections or resurrected pending results");
  automatic->setChecked(false);
  dialog.set_selected_radicals({0});
  require(!timer->isActive(), "Disabled radical auto search still scheduled work");
  automatic->setChecked(true);
  require(timer->isActive(), "Could not prepare a pending radical search before close");
  dialog.reject();
  QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
  require(!timer->isActive() && dialog.result_codes().empty(), "Hidden radical lookup still searched");
}

void test_artwork_palette_changes() {
  std::vector<std::vector<jwpqt::core::JisCode>> groups(241);
  groups[0] = {0x3021U};
  const auto radicals = lists(groups);
  groups.resize(30);
  const auto strokes = lists(groups);
  const auto info = information();
  QImage image(16, 241 * 16, QImage::Format_ARGB32);
  image.fill(Qt::white);
  image.setPixelColor(4, 8, Qt::black);
  image.setPixelColor(5, 8, QColor(0, 0, 0, 128));
  image.setPixelColor(14, 14, Qt::red);
  image.setPixelColor(15, 15, Qt::transparent);
  const QPixmap original = QPixmap::fromImage(image);
  const QImage before = original.toImage();
  jwpqt::qt::KanjiLookupDialog dialog(radicals, strokes, info, original, {}, {});
  dialog.findChild<QCheckBox*>(QStringLiteral("kanjiLookupAutoSearch"))->setChecked(false);
  dialog.set_selected_radicals({0});
  require(dialog.search(), "Could not prepare radical palette regression");
  dialog.show();
  auto* button = dialog.findChild<QToolButton*>(QStringLiteral("radicalButton1"));
  auto* timer = dialog.findChild<QTimer*>(QStringLiteral("kanjiLookupSearchTimer"));
  for (bool dark : {true, false, true, false}) {
    QPalette palette = dialog.palette();
    palette.setColor(QPalette::Window, dark ? QColor(32, 35, 37) : QColor(240, 240, 240));
    palette.setColor(QPalette::Text, dark ? QColor(240, 240, 240) : QColor(16, 16, 16));
    palette.setColor(QPalette::Button, palette.color(QPalette::Window));
    palette.setColor(QPalette::ButtonText, palette.color(QPalette::Text));
    dialog.setPalette(palette);
    QApplication::processEvents();
    const QImage icon = button->icon().pixmap(QSize(16, 16), 1.0).toImage();
    require(button->palette().color(QPalette::Button) == palette.color(QPalette::Button),
            "Radical button background retained the previous palette");
    require(icon.pixelColor(0, 0) == (dark ? palette.color(QPalette::Window) : QColor(Qt::white)) &&
                icon.pixelColor(4, 8) == (dark ? palette.color(QPalette::Text) : QColor(Qt::black)),
            "Radical bitmap does not follow the live light/dark palette");
    require(icon.pixelColor(5, 8).alpha() == 128 && icon.pixelColor(15, 15).alpha() == 0 &&
                icon.pixelColor(14, 14) == QColor(Qt::red) && original.toImage() == before,
            "Radical recoloring changed alpha, colored accents or the source sheet");
    const QImage selected = button->icon().pixmap(QSize(16, 16), 1.0, QIcon::Selected, QIcon::On).toImage();
    const QImage disabled = button->icon().pixmap(QSize(16, 16), 1.0, QIcon::Disabled).toImage();
    require(!selected.isNull() && !disabled.isNull() &&
                (!dark || (selected.pixelColor(4, 8).lightness() > selected.pixelColor(0, 0).lightness() &&
                           disabled.pixelColor(4, 8).alpha() < icon.pixelColor(4, 8).alpha())),
            "Selected or disabled dark radical artwork is unreadable");
    auto* heading = dialog.findChild<QLabel*>(QStringLiteral("radicalStrokeHeader1"));
    require(heading && heading->palette().color(QPalette::Window) ==
                (dark ? palette.color(QPalette::Window) : QColor(Qt::white)),
            "Radical stroke headings retained white paper in dark mode");
    require(dialog.selected_radicals() == std::vector<std::size_t>{0} &&
                dialog.result_codes() == std::vector<jwpqt::core::JisCode>{0x3021U} && !timer->isActive(),
            "Palette changes altered radical selection, results or search scheduling");
  }
  QPalette broken = dialog.palette();
  broken.setColor(QPalette::Window, QColor(32, 35, 37));
  broken.setColor(QPalette::Text, broken.color(QPalette::Window));
  timer->setInterval(60000);
  dialog.findChild<QCheckBox*>(QStringLiteral("kanjiLookupAutoSearch"))->setChecked(true);
  const int timer_id = timer->timerId();
  dialog.setPalette(broken);
  QApplication::processEvents();
  require(button->icon().pixmap(QSize(16, 16), 1.0).toImage().pixelColor(4, 8) == QColor(Qt::white) &&
              timer->isActive() && timer->timerId() == timer_id,
          "Unreadable theme ink was retained or palette changes restarted pending radical work");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  test_artwork_palette_changes();
  return EXIT_SUCCESS;
}
