// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>

#include <QApplication>
#include <QListWidget>
#include <QPushButton>

#include "jwpqt/core/kanji_info.h"
#include "kanji_code_lookup_dialog.h"

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

jwpqt::core::KanjiInfoDatabase database() {
  constexpr std::size_t variable = 28;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0x28U);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  bytes.resize(variable, '\0');
  put_u16(bytes, 12, 3U << 8U);
  put_u16(bytes, 14, (1U << 8U) | (2U << 11U));
  put_u16(bytes, 16, 3U);
  put_u16(bytes, 18, 1U);
  put_u32(bytes, 24, static_cast<std::uint32_t>(variable) << 8U);
  append_u16(bytes, 0U);
  append_u32(bytes, 0U);
  append_u32(bytes, 1234U << 6U | 5U << 20U);
  bytes.push_back('\0');
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

void test_dialog() {
  const auto source = database();
  std::vector<jwpqt::core::JisCode> inserted;
  jwpqt::core::JisCode shown = 0;
  jwpqt::qt::KanjiCodeLookupDialog dialog(
      source, [&](const auto& codes) { inserted = codes; },
      [&](jwpqt::core::JisCode code) { shown = code; });
  jwpqt::core::KanjiSkipQuery skip;
  skip.type = {1, 1};
  skip.first = {2, 2};
  skip.second = {3, 3};
  dialog.set_skip_query(skip);
  require(dialog.search_skip() && dialog.results().size() == 1 &&
              dialog.results()[0].code == 0x3021U,
          "Native SKIP dialog returned wrong results");
  auto* results =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiCodeResults"));
  results->item(0)->setSelected(true);
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeInsert"))->click();
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiCodeInfo"))->click();
  require(inserted == std::vector<jwpqt::core::JisCode>{0x3021U} &&
              shown == 0x3021U,
          "Native code lookup callbacks received wrong result");

  jwpqt::core::KanjiFourCornerQuery corner;
  corner.digits = {1, 2, 3, 4, 5};
  dialog.set_four_corner_query(corner);
  require(dialog.search_four_corner() && dialog.results().size() == 1,
          "Native four-corner dialog returned wrong results");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  return EXIT_SUCCESS;
}
