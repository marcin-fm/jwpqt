// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <string>

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>

#include "jwpqt/core/kanji_info.h"
#include "kanji_info_dialog.h"

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
  append_u32(bytes, 0U);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  bytes.resize(variable, '\0');
  put_u16(bytes, 12, 1U | (8U << 8U) | (1U << 13U));
  put_u16(bytes, 14, 3U | (1U << 4U));
  put_u16(bytes, 16, 1U << 6U);
  put_u16(bytes, 18, 100U << 1U);
  put_u16(bytes, 20, 200U << 1U);
  put_u32(bytes, 24, static_cast<std::uint32_t>(variable) << 8U);
  bytes.append("meaning\0", 8);
  bytes.append("\x22\0", 2);
  bytes.append("\x23\0", 2);
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

void test_dialog() {
  const auto source = database();
  jwpqt::qt::KanjiInfoDialog dialog(source);
  require(dialog.set_code(0x3021U) && dialog.code() == 0x3021U,
          "Kanji information dialog did not accept a database code");
  auto* character =
      dialog.findChild<QLabel*>(QStringLiteral("kanjiInfoCharacter"));
  auto* fields =
      dialog.findChild<QTableWidget*>(QStringLiteral("kanjiInfoFields"));
  auto* readings =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiInfoReadings"));
  auto* meanings =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiInfoMeanings"));
  require(character != nullptr && character->text() == QStringLiteral("\u4e9c") &&
              fields != nullptr && fields->rowCount() >= 8 &&
              readings != nullptr && readings->count() == 2 &&
              meanings != nullptr && meanings->count() == 1 &&
              meanings->item(0)->text() == QStringLiteral("meaning"),
          "Kanji information dialog rendered incomplete record data");
  require(!dialog.set_code(0x3022U) && dialog.code() == 0x3021U,
          "Unavailable kanji replaced the displayed record");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  return EXIT_SUCCESS;
}
