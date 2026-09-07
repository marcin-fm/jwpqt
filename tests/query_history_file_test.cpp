// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/query_history_file.h"
#include "jwpqt/core/utf8.h"

namespace {
using namespace jwpqt::core;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void fails(Function function) {
  try { function(); }
  catch (const QueryHistoryError&) { return; }
  throw std::runtime_error("Invalid history file was accepted");
}

using Entries = std::array<std::vector<std::u32string>, 3>;

std::string native(std::size_t capacity, const Entries& entries) {
  ByteWriter writer;
  writer.write_u32_le(kQueryHistoryMagic);
  writer.write_u32_le(static_cast<std::uint32_t>(capacity));
  for (const auto& list : entries) {
    writer.write_u32_le(static_cast<std::uint32_t>(list.size()));
    for (const auto& entry : list) {
      const auto text = encode_utf8(entry);
      writer.write_u32_le(static_cast<std::uint32_t>(text.size()));
      writer.write_bytes(text);
    }
  }
  return writer.take_bytes();
}

void set_u16(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes.at(offset) = static_cast<char>(value & 255U);
  bytes.at(offset + 1) = static_cast<char>(value >> 8U);
}

void set_u32(std::string& bytes, std::size_t offset, std::uint32_t value) {
  set_u16(bytes, offset, static_cast<std::uint16_t>(value & 65535U));
  set_u16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

std::string legacy(std::size_t capacity, const std::array<std::vector<JwpText>, 3>& entries) {
  ByteWriter writer;
  writer.write_u32_le(kLegacyQueryHistoryMagic);
  for (const auto& list : entries) {
    writer.write_u32_le(static_cast<std::uint32_t>(list.size()));
    std::vector<std::uint16_t> cells(capacity, 0xffff);
    std::size_t end = 0;
    if (!list.empty()) cells.at(0) = 0;
    for (std::size_t i = 0; i < list.size(); ++i) {
      for (const auto token : list[i]) cells.at(capacity / 10 + 2 + end++) = token;
      cells.at(i + 1) = static_cast<std::uint16_t>(end);
    }
    for (const auto cell : cells) writer.write_u16_le(cell);
  }
  return writer.take_bytes();
}

void test_native() {
  const Entries entries{{{U"new", U"\ufeff\ufffe\u00a0\U0001f680\told"},
                          {U"search", U"Search"}, {U"replace"}}};
  const auto bytes = native(300, entries);
  const auto loaded = parse_query_history_file(bytes);
  require(bytes.substr(0, 4) == "JQH1" && loaded.dictionary.storage_cells() == 300 &&
              loaded.dictionary.entries() == entries[0] && loaded.search.entries() == entries[1] &&
              loaded.replace.entries() == entries[2] && encode_query_history_file(loaded) == bytes,
          "Native histories lost order, text or another history kind");
  for (std::size_t n = 0; n < bytes.size(); ++n)
    fails([&] { parse_query_history_file(std::string_view(bytes).substr(0, n)); });
  fails([&] { parse_query_history_file(bytes + "x"); });
  auto corrupt = bytes;
  corrupt[3] = '2';
  fails([&] { parse_query_history_file(corrupt); });
  corrupt = bytes; set_u32(corrupt, 4, 30001);
  fails([&] { parse_query_history_file(corrupt); });
  corrupt = bytes; set_u32(corrupt, 8, 0xffffffff);
  fails([&] { parse_query_history_file(corrupt); });
  corrupt = bytes; set_u32(corrupt, 12, 0xffffffff);
  fails([&] { parse_query_history_file(corrupt); });
  corrupt = bytes; corrupt[16] = static_cast<char>(0xff);
  fails([&] { parse_query_history_file(corrupt); });
  corrupt = bytes; corrupt.replace(16, 3, "\xed\xa0\x80");
  fails([&] { parse_query_history_file(corrupt); });
  for (const auto& text : std::vector<std::u32string>{
           U"", U"\n", std::u32string(1, U'\0'), std::u32string(U"x\0y", 3),
           U"\u0085", U"\u2028", U"\u2029"})
    fails([&] { parse_query_history_file(native(300, Entries{{{text}, {}, {}}})); });
  fails([&] { parse_query_history_file(native(300, Entries{{{U"same", U"same"}, {}, {}}})); });
  fails([&] { parse_query_history_file(native(10, Entries{{{U"abcd", U"efg"}, {}, {}}})); });
  Entries maximum;
  for (auto& list : maximum) list.push_back(std::u32string(26997, U'\U0001f680'));
  const auto large = parse_query_history_file(native(30000, maximum));
  require(large.dictionary.entries() == maximum[0] && large.search.entries() == maximum[1] &&
              large.replace.entries() == maximum[2], "Per-history scalar budgets were conflated");
  maximum[2][0].push_back(U'x');
  fails([&] { parse_query_history_file(native(30000, maximum)); });
  for (std::size_t capacity = 0; capacity < 4; ++capacity) {
    const auto empty = parse_query_history_file(encode_query_history_file(QueryHistories(capacity)));
    require(empty.dictionary.storage_cells() == capacity && empty.dictionary.entries().empty(),
            "Disabled history capacity did not round trip");
    fails([&] { parse_query_history_file(native(capacity, Entries{{{U"x"}, {}, {}}})); });
  }
  QueryHistories mismatch;
  mismatch.search.set_storage_cells(10);
  fails([&] { encode_query_history_file(mismatch); });
  std::string too_large(kMaximumQueryHistoryFileBytes, 'x');
  fails([&] { parse_query_history_file(too_large); });
  too_large.push_back('x');
  fails([&] { parse_query_history_file(too_large); });
}

void test_legacy() {
  const std::array<std::vector<JwpText>, 3> entries{{{{0x80}, {'o', 'l', 'd'}},
                                                   {{0x2422, '\t', 0x3026}}, {{'r'}}}};
  const auto bytes = legacy(300, entries);
  const auto loaded = parse_legacy_query_history_file(bytes, 300, LegacyCodePage::k1251);
  require(loaded.dictionary.entries() == std::vector<std::u32string>{U"\u0402", U"old"} &&
              loaded.search.entries() == std::vector<std::u32string>{U"\u3042\t\u611b"} &&
              loaded.replace.entries() == std::vector<std::u32string>{U"r"},
          "Legacy histories lost code page, JIS, order or another history kind");
  require(parse_query_history_file(encode_query_history_file(loaded)).search.entries() ==
              loaded.search.entries(), "Imported histories did not survive native storage");
  // The recovered CP1252 table leaves 0x80 undefined; do not substitute Euro.
  fails([&] { parse_legacy_query_history_file(bytes, 300, LegacyCodePage::k1252); });
  for (std::size_t n = 0; n < bytes.size(); ++n)
    fails([&] { parse_legacy_query_history_file(std::string_view(bytes).substr(0, n), 300,
                                              LegacyCodePage::k1251); });
  const auto with_tail = bytes + std::string("\0\xffpaths", 7);
  require(parse_legacy_query_history_file(with_tail, 300, LegacyCodePage::k1251)
              .dictionary.entries() == loaded.dictionary.entries(), "Legacy path tail was misparsed");
  fails([&] { parse_legacy_query_history_file(bytes, 30001, LegacyCodePage::k1251); });
  fails([&] { parse_legacy_query_history_file(bytes, 299, LegacyCodePage::k1251); });
  fails([&] { parse_legacy_query_history_file(bytes, std::numeric_limits<std::size_t>::max(),
                                            LegacyCodePage::k1251); });
  fails([&] { parse_legacy_query_history_file(bytes, 300, static_cast<LegacyCodePage>(0)); });
  auto corrupt = bytes; corrupt[3] = 0;
  fails([&] { parse_legacy_query_history_file(corrupt, 300, LegacyCodePage::k1251); });
  for (const auto count : {32U, 0xffffffffU}) {
    corrupt = bytes; set_u32(corrupt, 4, count);
    fails([&] { parse_legacy_query_history_file(corrupt, 300, LegacyCodePage::k1251); });
  }
  for (const auto offset : {0U, 268U, 65535U}) {
    corrupt = bytes; set_u16(corrupt, 10, static_cast<std::uint16_t>(offset));
    fails([&] { parse_legacy_query_history_file(corrupt, 300, LegacyCodePage::k1251); });
  }
  corrupt = bytes; set_u16(corrupt, 8, 1);
  fails([&] { parse_legacy_query_history_file(corrupt, 300, LegacyCodePage::k1251); });
  corrupt = bytes; set_u16(corrupt, 8 + 64, 0xffff);
  fails([&] { parse_legacy_query_history_file(corrupt, 300, LegacyCodePage::k1251); });
  fails([&] { parse_legacy_query_history_file(legacy(300, {{{{'a'}, {'a'}}, {}, {}}}),
                                            300, LegacyCodePage::k1252); });
  std::array<std::vector<JwpText>, 3> full;
  full[0].push_back(JwpText(267, 'a'));
  require(parse_legacy_query_history_file(legacy(300, full), 300, LegacyCodePage::k1252)
              .dictionary.entries().front().size() == 267, "Exact legacy text limit was rejected");
  full[0].clear();
  for (int i = 0; i < 31; ++i) full[0].push_back({static_cast<JisCode>('A' + i)});
  require(parse_legacy_query_history_file(legacy(300, full), 300, LegacyCodePage::k1252)
              .dictionary.entries().size() == 31, "Exact legacy entry limit was rejected");
  for (std::size_t capacity = 0; capacity < 4; ++capacity)
    require(parse_legacy_query_history_file(legacy(capacity, {}), capacity, LegacyCodePage::k1252)
                .dictionary.entries().empty(), "Unused legacy cells were interpreted as text");
  auto bounded_tail = bytes;
  bounded_tail.resize(kMaximumQueryHistoryFileBytes, '\0');
  require(parse_legacy_query_history_file(bounded_tail, 300, LegacyCodePage::k1251)
              .replace.entries() == loaded.replace.entries(), "Exact byte limit was rejected");
  fails([&] { parse_legacy_query_history_file(std::string(kMaximumQueryHistoryFileBytes + 1, 'x'),
                                            300, LegacyCodePage::k1252); });
}

}  // namespace

int main() {
  try { test_native(); test_legacy(); }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
  return EXIT_SUCCESS;
}
