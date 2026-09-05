// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "jwpqt/core/kanji_lookup_lists.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

template <typename Function>
void require_error(Function&& function, const char* message) {
  try {
    function();
  } catch (const jwpqt::core::KanjiLookupListError&) {
    return;
  }
  require(false, message);
}

void append_u16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

std::string fixture() {
  std::string bytes;
  append_u16(bytes, 8U);
  append_u16(bytes, 2U);
  append_u16(bytes, 12U);
  append_u16(bytes, 1U);
  append_u16(bytes, 0x3021U);
  append_u16(bytes, 0x3022U);
  append_u16(bytes, 0x3023U);
  return bytes;
}

void test_parse() {
  const auto lists = jwpqt::core::KanjiLookupLists::parse(fixture(), 2U);
  require(lists.group_count() == 2 && lists.membership_count() == 3 &&
              lists.group(0) ==
                  std::vector<jwpqt::core::JisCode>{0x3021U, 0x3022U} &&
              lists.group(1) ==
                  std::vector<jwpqt::core::JisCode>{0x3023U},
          "Kanji lookup groups decoded incorrectly");
  require_error([&] { (void)lists.group(2); },
                "Out-of-range kanji lookup group was accepted");
}

void test_malformed_and_limits() {
  std::string bytes = fixture();
  bytes[4] = 11;
  require_error(
      [&] { (void)jwpqt::core::KanjiLookupLists::parse(bytes, 2U); },
      "Non-contiguous kanji lookup group was accepted");
  bytes = fixture();
  bytes.push_back('\0');
  require_error(
      [&] { (void)jwpqt::core::KanjiLookupLists::parse(bytes, 2U); },
      "Trailing kanji lookup bytes were accepted");
  bytes = fixture();
  bytes[8] = 'A';
  bytes[9] = 0;
  require_error(
      [&] { (void)jwpqt::core::KanjiLookupLists::parse(bytes, 2U); },
      "Non-kanji lookup member was accepted");

  jwpqt::core::KanjiLookupListLimits limits;
  limits.memberships = 2;
  require_error(
      [&] { (void)jwpqt::core::KanjiLookupLists::parse(fixture(), 2U, limits); },
      "Kanji lookup membership limit was not enforced");
  limits = {};
  limits.groups = 1;
  require_error(
      [&] { (void)jwpqt::core::KanjiLookupLists::parse(fixture(), 2U, limits); },
      "Kanji lookup group limit was not enforced");
}

std::string read_file(const char* path) {
  std::ifstream input(path, std::ios::binary);
  require(static_cast<bool>(input), "Could not open recovered lookup file");
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void test_recovered_files(const char* radical_path, const char* stroke_path) {
  const auto radicals =
      jwpqt::core::parse_radical_lists(read_file(radical_path));
  const auto strokes = jwpqt::core::parse_stroke_lists(read_file(stroke_path));
  require(radicals.group_count() == 241 &&
              radicals.membership_count() == 22'788 &&
              strokes.group_count() == 30 &&
              strokes.membership_count() == 6'355,
          "Recovered kanji lookup files have unexpected dimensions");
}

}  // namespace

int main(int argc, char* argv[]) {
  test_parse();
  test_malformed_and_limits();
  if (argc == 3) test_recovered_files(argv[1], argv[2]);
  return EXIT_SUCCESS;
}
