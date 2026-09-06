// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>

#include <QApplication>
#include <QListWidget>
#include <QPushButton>

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
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  return EXIT_SUCCESS;
}
