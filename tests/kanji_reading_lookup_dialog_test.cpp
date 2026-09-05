// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

#include "jwpqt/core/kanji_info.h"
#include "kanji_reading_lookup_dialog.h"

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

void append_string(std::string& bytes, std::string_view value) {
  bytes.append(value);
  bytes.push_back('\0');
}

jwpqt::core::KanjiInfoDatabase database() {
  constexpr std::size_t variable = 28;
  std::string bytes;
  append_u32(bytes, jwpqt::core::kKanjiInfoMagic);
  append_u32(bytes, 0x3fU);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  bytes.resize(variable, '\0');
  put_u16(bytes, 12, (4U << 8U) | (1U << 13U));
  put_u16(bytes, 14, 1U << 4U);
  put_u16(bytes, 16, (1U << 5U) | (1U << 6U) | (1U << 11U));
  put_u16(bytes, 20, 1U);
  put_u32(bytes, 24, static_cast<std::uint32_t>(bytes.size()) << 8U);
  append_string(bytes, "han");
  append_string(bytes, std::string("m\xe0", 2));
  append_string(bytes, "red apple");
  append_string(bytes, std::string("\x22", 1));
  append_string(bytes, std::string("\x22\xa4", 2));
  append_string(bytes, std::string("\x2a", 1));
  return jwpqt::core::KanjiInfoDatabase::parse(bytes);
}

void test_dialog() {
  const auto source = database();
  std::vector<jwpqt::core::JisCode> inserted;
  jwpqt::core::JisCode shown = 0;
  jwpqt::qt::KanjiReadingLookupDialog dialog(
      source, [&](const auto& codes) { inserted = codes; },
      [&](jwpqt::core::JisCode code) { shown = code; });
  jwpqt::core::KanjiReadingQuery query;
  query.kind = jwpqt::core::KanjiReadingKind::kMeaning;
  query.text = U"apple";
  query.strokes = {4, 4};
  dialog.set_query(query);
  auto* flexible = dialog.findChild<QCheckBox*>(
      QStringLiteral("kanjiReadingFlexibleKun"));
  auto* partial = dialog.findChild<QCheckBox*>(
      QStringLiteral("kanjiReadingPartialWords"));
  require(flexible != nullptr && partial != nullptr &&
              !flexible->isEnabled() && partial->isEnabled(),
          "Native reading mode did not gate its matching options");
  require(dialog.search() &&
              dialog.results() ==
                  std::vector<jwpqt::core::JisCode>{0x3021U},
          "Native reading dialog returned wrong meaning results");

  auto* results =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiReadingResults"));
  require(results != nullptr && results->count() == 1,
          "Native reading dialog did not render results");
  results->item(0)->setSelected(true);
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiReadingInsert"))->click();
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiReadingInfo"))->click();
  require(inserted == std::vector<jwpqt::core::JisCode>{0x3021U} &&
              shown == 0x3021U,
          "Native reading callbacks received wrong result");

  query.kind = jwpqt::core::KanjiReadingKind::kKun;
  query.text = U"あ";
  query.strokes = {0, 30};
  dialog.set_query(query);
  require(flexible->isEnabled() && !partial->isEnabled(),
          "Kana reading mode did not gate its matching options");
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiReadingSearch"))->click();
  require(dialog.results().size() == 1,
          "Native reading dialog did not run a kana lookup");
  query.text = U"not kana";
  dialog.set_query(query);
  require(!dialog.search() && dialog.results().size() == 1,
          "Failed native reading search discarded prior results");
}

void test_callback_containment() {
  const auto source = database();
  jwpqt::qt::KanjiReadingLookupDialog dialog(
      source, [](const auto&) { throw std::runtime_error("insert failed"); },
      [](auto) { throw 1; });
  jwpqt::core::KanjiReadingQuery query;
  query.kind = jwpqt::core::KanjiReadingKind::kMeaning;
  query.text = U"apple";
  dialog.set_query(query);
  require(dialog.search(), "Could not prepare callback containment fixture");
  auto* results =
      dialog.findChild<QListWidget*>(QStringLiteral("kanjiReadingResults"));
  results->item(0)->setSelected(true);
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiReadingInsert"))->click();
  auto* status =
      dialog.findChild<QLabel*>(QStringLiteral("kanjiReadingStatus"));
  require(status != nullptr && status->text().contains("insert failed"),
          "Native reading insertion exception escaped its UI boundary");
  dialog.findChild<QPushButton*>(QStringLiteral("kanjiReadingInfo"))->click();
  require(status->text().contains("failed", Qt::CaseInsensitive),
          "Unknown native reading information failure was not contained");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_dialog();
  test_callback_containment();
  return EXIT_SUCCESS;
}
