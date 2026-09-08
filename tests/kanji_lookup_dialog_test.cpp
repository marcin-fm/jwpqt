// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QEventLoop>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>

#include "jwpqt/core/kanji_lookup_lists.h"
#include "kanji_lookup_dialog.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

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

void test_rare_preference() {
  const auto radicals = lists({{0x3021U}});
  std::vector<std::vector<jwpqt::core::JisCode>> rows(30);
  rows[0] = {0x5021U, 0x3021U};
  const auto strokes = lists(rows);
  const auto info = information();
  jwpqt::qt::KanjiLookupDialog dialog(radicals, strokes, info, {}, {}, {});
  require(dialog.search() && dialog.result_codes() == jwpqt::core::JwpText({0x3021U, 0x5021U}),
          "Rare-last default did not group radical results");
  auto* list = dialog.findChild<QListWidget*>("kanjiLookupResults");
  list->setCurrentRow(1);
  dialog.set_lookup_options(false, false);
  require(list->currentRow() == 1 && dialog.result_codes() == jwpqt::core::JwpText({0x3021U, 0x5021U}),
          "Changing lookup preferences reordered or deselected current results");
  require(dialog.search() && dialog.result_codes() == jwpqt::core::JwpText({0x5021U, 0x3021U}),
          "Rare-last preference did not reach the next search");
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

void test_stroke_and_clipboard_controls() {
  using namespace jwpqt;
  std::vector<std::vector<core::JisCode>> groups(241);
  groups[0] = {0x3021};
  groups[64] = {0x3022};
  groups[186] = {0x3022};
  const auto radicals = lists(groups);
  groups.assign(30, {});
  groups[0] = {0x3021}; groups[1] = {0x3022};
  const auto strokes = lists(groups);
  const auto info = information();
  qt::KanjiLookupDialog dialog(radicals, strokes, info, {}, {}, {});
  auto* automatic = dialog.findChild<QCheckBox*>(QStringLiteral("kanjiLookupAutoSearch"));
  auto* count = dialog.findChild<QSpinBox*>(QStringLiteral("kanjiLookupStrokeCount"));
  auto* tolerance = dialog.findChild<QComboBox*>(QStringLiteral("kanjiLookupTolerance"));
  auto* minimum = dialog.findChild<QSpinBox*>(QStringLiteral("minimumStrokes"));
  auto* maximum = dialog.findChild<QSpinBox*>(QStringLiteral("maximumStrokes"));
  auto* paste = dialog.findChild<QPushButton*>(QStringLiteral("kanjiLookupFromClipboard"));
  auto* timer = dialog.findChild<QTimer*>(QStringLiteral("kanjiLookupSearchTimer"));
  require(count && tolerance && paste, "Radical quick-count/tolerance/clipboard controls missing");
  automatic->setChecked(false);
  dialog.show();
  dialog.set_selected_radicals({32});
  require(dialog.selected_radicals() == std::vector<std::size_t>({32, 186}) &&
              count->value() == 0 && minimum->value() == 1 && maximum->value() == 30,
          "Radical variants did not link or selection unexpectedly filtered stroke counts");
  count->stepUp();
  require(count->value() == 9 && minimum->value() == 9 && maximum->value() == 9,
          "Radical spinner did not skip to the selected stroke estimate");
  count->stepDown(); require(count->value() == 0, "Smart stroke decrement did not return to Any");
  count->stepDown(); require(count->value() == 30, "Smart stroke spinner did not wrap backwards");
  tolerance->setCurrentIndex(2);
  require(minimum->value() == 28 && maximum->value() == 30, "Upper stroke tolerance was not clipped");
  count->setValue(1);
  require(minimum->value() == 1 && maximum->value() == 3, "Lower stroke tolerance was not clipped");
  count->setValue(2); tolerance->setCurrentIndex(0);
  require(dialog.search() && dialog.result_codes() == std::vector<core::JisCode>({0x3022}),
          "Quick exact stroke search did not use the selected radical variants");
  dialog.set_stroke_range(5, 8);
  require(count->value() == 0 && count->text().contains(QStringLiteral("Custom")) &&
              minimum->value() == 5 && maximum->value() == 8 && dialog.result_codes().empty(),
          "Custom range was overwritten by stale quick-count state");
  minimum->setValue(8);
  require(count->value() == 8 && maximum->value() == 8, "Manual exact range did not synchronize quick count");
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiLookupAnyStrokes"))->click();
  require(count->value() == 0 && minimum->value() == 1 && maximum->value() == 30,
          "Any strokes did not reset both control paths");
  dialog.findChild<QToolButton*>(QStringLiteral("radicalButton33"))->click();
  require(dialog.selected_radicals().empty(), "Toggling a variant left its linked form selected");
  QApplication::clipboard()->setText(qt::to_qstring(core::decode_jwp_text({0x3022})) + QStringLiteral(" trailing"));
  paste->click();
  require(dialog.selected_radicals() == std::vector<std::size_t>({32, 64, 65, 66, 186}) &&
              !timer->isActive() && dialog.search() && dialog.result_codes().size() == 1,
          "Clipboard extraction failed to select linked radicals from the first kanji");
  const auto selected = dialog.selected_radicals();
  const auto results = dialog.result_codes();
  for (const auto& text : {QString{}, QStringLiteral("abc"), QString::fromUtf8("\xe3\x81\x82"),
                          QString::fromUtf8("\xf0\x9f\x98\x80")}) {
    QApplication::clipboard()->setText(text); paste->click();
    require(dialog.selected_radicals() == selected && dialog.result_codes() == results,
            "Invalid clipboard extraction erased current radical state/results");
  }
  require(!dialog.select_kanji(0x3023) && dialog.selected_radicals() == selected,
          "Kanji absent from radical data erased current selection");
  automatic->setChecked(true);
  require(dialog.select_kanji(0x3021) && timer->isActive(), "Extracting kanji did not schedule enabled search");
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiLookupClear"))->click();
  require(count->value() == 0 && tolerance->currentIndex() == 0 &&
              dialog.selected_radicals().empty() && dialog.result_codes().empty() && !timer->isActive(),
          "Clear did not reset quick controls, variants and pending search");
  dialog.grab().save(QStringLiteral("radical-stroke-controls.png"));

  for (bool insert : {false, true}) {
    qt::KanjiLookupDialog* owner = nullptr;
    const auto destroy = [&] { delete owner; owner = nullptr; throw std::runtime_error("Owner closed"); };
    owner = new qt::KanjiLookupDialog(radicals, strokes, info, {},
        [&](const auto&) { destroy(); }, [&](auto) { destroy(); });
    owner->set_selected_radicals({0});
    require(owner->search(), "Could not prepare radical callback lifetime test");
    owner->findChild<QPushButton*>(insert ? QStringLiteral("kanjiLookupInsert") :
                                           QStringLiteral("kanjiLookupInfo"))->click();
    require(!owner, "Radical callback did not close its owner safely");
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  test_rare_preference();
  test_artwork_palette_changes();
  test_stroke_and_clipboard_controls();
  return EXIT_SUCCESS;
}
