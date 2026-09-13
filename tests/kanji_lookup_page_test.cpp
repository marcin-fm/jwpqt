// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QImage>
#include <QPalette>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>

#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/kanji_lookup_lists.h"
#include "kanji_lookup_page.h"
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
  for (std::size_t index = 0; index < groups.size(); ++index) {
    put_u16(bytes, index * 4U, static_cast<std::uint16_t>(offset));
    put_u16(bytes, index * 4U + 2U,
            static_cast<std::uint16_t>(groups[index].size()));
    for (const auto code : groups[index]) append_u16(bytes, code);
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
  for (std::size_t index = 0; index < count; ++index) {
    put_u16(bytes, 12U + index * 16U,
            static_cast<std::uint16_t>((index + 1U) << 8U));
    put_u32(bytes, 12U + index * 16U + 12U,
            static_cast<std::uint32_t>(variable) << 8U);
  }
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

struct Fixtures {
  jwpqt::core::KanjiLookupLists radicals;
  jwpqt::core::KanjiLookupLists strokes;
  jwpqt::core::KanjiInfoDatabase info;
};

Fixtures fixtures() {
  std::vector<std::vector<jwpqt::core::JisCode>> radicals(
      jwpqt::core::kRadicalListGroups);
  radicals[0] = {0x3021U, 0x3022U};
  radicals[1] = {0x3022U};
  radicals[64] = {0x3022U};
  radicals[186] = {0x3022U};
  std::vector<std::vector<jwpqt::core::JisCode>> strokes(
      jwpqt::core::kStrokeListGroups);
  strokes[0] = {0x3021U};
  strokes[1] = {0x3022U};
  strokes[2] = {0x3023U};
  return {lists(radicals), lists(strokes), information()};
}

void test_radical_criteria() {
  const Fixtures source = fixtures();
  jwpqt::qt::KanjiLookupPage page(
      source.radicals, source.strokes, source.info, {},
      jwpqt::qt::KanjiLookupPageMode::kRadical);
  require(page.findChild<QSpinBox*>("kanjiRadicalLookupStrokeCount") &&
              !page.findChild<QPushButton*>("kanjiRadicalLookupSearch"),
          "Radical tab is not a criteria-only page beginning at stroke count");

  int changes = 0;
  page.set_change_handler([&] { ++changes; });
  page.set_selected_radicals({0, 1});
  const auto report = page.search(true);
  require(report.results.size() == 1 && report.results[0].code == 0x3022U,
          "Radical intersection did not produce the expected result");
  try {
    page.set_selected_radicals({jwpqt::core::kRadicalListGroups});
    require(false, "Out-of-range radical selection was accepted");
  } catch (const jwpqt::core::KanjiLookupListError&) {
  }
  require(page.selected_radicals() == std::vector<std::size_t>({0, 1}) &&
              changes == 1,
          "Rejected radical criteria changed page state");

  auto* count = page.findChild<QSpinBox*>("kanjiRadicalLookupStrokeCount");
  auto* tolerance =
      page.findChild<QComboBox*>("kanjiRadicalLookupTolerance");
  auto* minimum =
      page.findChild<QSpinBox*>("kanjiRadicalLookupMinimumStrokes");
  auto* maximum =
      page.findChild<QSpinBox*>("kanjiRadicalLookupMaximumStrokes");
  require(count && tolerance && minimum && maximum,
          "Radical stroke controls are incomplete");
  count->setValue(2);
  tolerance->setCurrentIndex(1);
  require(minimum->value() == 1 && maximum->value() == 3,
          "Radical tolerance did not update the stroke range");
  page.set_stroke_range(2, 2);
  require(count->value() == 2 && tolerance->currentIndex() == 0,
          "Explicit stroke range did not synchronize controls");
  page.clear();
  require(page.selected_radicals().empty() && count->value() == 0 &&
              minimum->value() == 1 && maximum->value() == 30,
          "Radical criteria Clear did not restore defaults");
}

void test_clipboard_and_stroke_page() {
  const Fixtures source = fixtures();
  QString error;
  jwpqt::qt::KanjiLookupPage radical(
      source.radicals, source.strokes, source.info, {},
      jwpqt::qt::KanjiLookupPageMode::kRadical);
  radical.set_error_handler([&](const QString& message) { error = message; });
  QApplication::clipboard()->setText(
      jwpqt::qt::to_qstring(jwpqt::core::decode_jwp_text({0x3022U})) +
      QStringLiteral(" trailing"));
  radical.findChild<QPushButton*>(
      "kanjiRadicalLookupFromClipboard")->click();
  require(!radical.selected_radicals().empty() && error.isEmpty(),
          "Radical clipboard seeding did not select source radicals");
  const auto selected = radical.selected_radicals();
  QApplication::clipboard()->setText(QStringLiteral("abc"));
  radical.findChild<QPushButton*>(
      "kanjiRadicalLookupFromClipboard")->click();
  require(radical.selected_radicals() == selected && !error.isEmpty(),
          "Invalid clipboard input erased radical criteria");

  jwpqt::qt::KanjiLookupPage stroke(
      source.radicals, source.strokes, source.info, {},
      jwpqt::qt::KanjiLookupPageMode::kStrokeCount);
  require(!stroke.findChild<QToolButton*>("radicalButton1") &&
              stroke.findChild<QSpinBox*>("kanjiStrokeLookupStrokeCount"),
          "Stroke Count page contains radical-only controls");
  stroke.set_stroke_range(3, 3);
  const auto report = stroke.search(false);
  require(report.results.size() == 1 && report.results[0].code == 0x3023U &&
              report.results[0].strokes == 3,
          "Stroke Count criteria did not produce the requested row");
}

void test_artwork_palette_changes() {
  const Fixtures source = fixtures();
  QImage image(16, static_cast<int>(jwpqt::core::kRadicalListGroups) * 16,
               QImage::Format_ARGB32);
  image.fill(Qt::white);
  image.setPixelColor(4, 8, Qt::black);
  image.setPixelColor(14, 14, Qt::red);
  image.setPixelColor(15, 15, Qt::transparent);
  const QPixmap original = QPixmap::fromImage(image);
  const QImage before = original.toImage();
  jwpqt::qt::KanjiLookupPage page(
      source.radicals, source.strokes, source.info, original,
      jwpqt::qt::KanjiLookupPageMode::kRadical);
  page.set_selected_radicals({0});
  page.show();
  auto* button = page.findChild<QToolButton*>("radicalButton1");
  for (bool dark : {true, false, true, false}) {
    QPalette palette = page.palette();
    palette.setColor(QPalette::Window,
                     dark ? QColor(32, 35, 37) : QColor(240, 240, 240));
    palette.setColor(QPalette::Text,
                     dark ? QColor(240, 240, 240) : QColor(16, 16, 16));
    palette.setColor(QPalette::Button, palette.color(QPalette::Window));
    palette.setColor(QPalette::ButtonText, palette.color(QPalette::Text));
    page.setPalette(palette);
    QApplication::processEvents();
    const QImage icon = button->icon().pixmap(QSize(16, 16), 1.0).toImage();
    require(icon.pixelColor(0, 0) ==
                    (dark ? palette.color(QPalette::Window) : QColor(Qt::white)) &&
                icon.pixelColor(4, 8) ==
                    (dark ? palette.color(QPalette::Text) : QColor(Qt::black)) &&
                icon.pixelColor(14, 14) == QColor(Qt::red) &&
                icon.pixelColor(15, 15).alpha() == 0 &&
                original.toImage() == before &&
                page.selected_radicals() == std::vector<std::size_t>{0},
            "Radical artwork lost palette, source or selection state");
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_radical_criteria();
  test_clipboard_and_stroke_page();
  test_artwork_palette_changes();
  return EXIT_SUCCESS;
}
