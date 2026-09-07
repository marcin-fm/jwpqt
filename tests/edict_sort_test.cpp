// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_sort.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/jis_unicode.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

using namespace jwpqt::core;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

EdictRecord record(std::u32string word, std::u32string reading,
                   std::u32string definition = U"meaning") {
  EdictRecord result;
  result.headword = std::move(word);
  if (!reading.empty()) result.readings.push_back(std::move(reading));
  result.definitions.push_back(std::move(definition));
  return result;
}

std::vector<std::size_t> sorted(const std::vector<EdictRecord>& records,
                                 EdictSortOptions options = {}, EdictSortLimits limits = {}) {
  std::vector<std::reference_wrapper<const EdictRecord>> references;
  for (const auto& entry : records) references.push_back(std::cref(entry));
  return sort_edict_records(references, options, limits);
}

void reject(const std::vector<EdictRecord>& records, EdictSortOptions options,
            EdictSortLimits limits) {
  const auto before = records;
  bool failed = false;
  try { (void)sorted(records, options, limits); }
  catch (const EdictSortError&) { failed = true; }
  require(failed && records == before, "Invalid or over-budget sort succeeded or changed input");
}

void test_reading_and_entry() {
  const std::vector<EdictRecord> readings = {
      record(U"A", U"\u30ab"), record(U"B", U"\u3042"),
      record(U"Z", U"\u304b"), record(U"C", U"\u304d")};
  EdictSortOptions options;
  require(sorted(readings, options) == std::vector<std::size_t>{1, 2, 0, 3},
          "Reading sort lost low-byte kana order or hiragana/katakana ties");
  options.reverse = true;
  require(sorted(readings, options) == std::vector<std::size_t>{3, 0, 2, 1},
          "Reverse reading order was incorrect");
  auto multiple = readings;
  auto entry = record(U"D", U"\u304b");
  entry.readings.push_back(U"\u3042");
  multiple.push_back(entry);
  options.reverse = false;
  require(sorted(multiple, options) == std::vector<std::size_t>{1, 4, 2, 0, 3},
          "Reading sort did not use the first reading block and complete-entry tie");

  const auto first = jis_x0208_to_unicode(0x3024);
  const auto second = jis_x0208_to_unicode(0x3025);
  require(first && second, "JIS sort fixture is unavailable");
  options.mode = EdictSortMode::kEntry;
  const std::vector<EdictRecord> jis = {record(std::u32string(1, *second), {}),
                                       record(std::u32string(1, *first), {})};
  require(sorted(jis, options) == std::vector<std::size_t>{1, 0},
          "Entry sorting replaced JIS collation with Unicode or locale ordering");
  const std::vector<EdictRecord> definitions = {record(U"z", {}, U"a"), record(U"a", {}, U"z")};
  require(sorted(definitions, options) == std::vector<std::size_t>{1, 0}, "Entry ordering failed");
  options.mode = EdictSortMode::kDefinition;
  require(sorted(definitions, options) == std::vector<std::size_t>{0, 1}, "Definition ordering failed");

  std::vector<EdictRecord> priority = {record(U"word", {}, U"a"),
                                     record(U"word", {}, U"a"), record(U"word", {}, U"b")};
  priority[1].definitions.push_back(U"(P)");
  for (const auto mode : {EdictSortMode::kEntry, EdictSortMode::kDefinition}) {
    options.mode = mode;
    options.reverse = false;
    require(sorted(priority, options) == std::vector<std::size_t>{1, 0, 2},
            "Terminal priority marker did not precede its otherwise identical entry");
    options.reverse = true;
    require(sorted(priority, options) == std::vector<std::size_t>{2, 0, 1},
            "Reverse priority comparison failed");
  }
  require(sorted(priority) == std::vector<std::size_t>{0, 1, 2},
          "Reading ties incorrectly inherited Entry's priority exception");
}

void test_length_and_cycles() {
  EdictSortOptions options;
  options.mode = EdictSortMode::kLength;
  const std::vector<EdictRecord> tied = {record(U"a", U"\u3042\u3042"),
      record(U"b", U"\u3042\u3042"), record(U"cc", U"\u3042")};
  require(sorted(tied, options) == std::vector<std::size_t>{0, 1, 2}, "Descending reading length failed");
  options.reverse = true;
  require(sorted(tied, options) == std::vector<std::size_t>{2, 0, 1},
          "Reverse Length reversed the ascending Entry tie rule");
  const std::vector<EdictRecord> same_reading = {record(U"z", U"\u3042"), record(U"aa", U"\u3042")};
  options.reverse = false;
  require(sorted(same_reading, options) == std::vector<std::size_t>{1, 0},
          "Identical readings did not use headword lengths");
  options.reverse = true;
  require(sorted(same_reading, options) == std::vector<std::size_t>{0, 1},
          "Reverse identical-reading headword length failed");
  options.headword_length = true;
  const std::vector<EdictRecord> heads = {record(U"a", U"\u3042\u3042"),
      record(U"a", U"\u3042"), record(U"bb", U"\u3042")};
  options.reverse = false;
  require(sorted(heads, options) == std::vector<std::size_t>{2, 0, 1},
          "Headword length and same-headword reading fallback failed");
  options.reverse = true;
  require(sorted(heads, options) == std::vector<std::size_t>{1, 0, 2},
          "Reverse headword/readings lengths failed");
  const std::vector<EdictRecord> different = {record(U"ab", U"a"), record(U"cd", U"aaa")};
  require(sorted(different, options) == std::vector<std::size_t>{0, 1},
          "Equal distinct headwords incorrectly sorted by reading length");

  const std::vector<EdictRecord> cycle = {record(U"word", {}, U"a"),
      record(U"word", {}, U"a, (P)"), record(U"word", {}, U"a,")};
  for (const auto mode : {EdictSortMode::kEntry, EdictSortMode::kDefinition}) {
    options = {};
    options.mode = mode;
    require(sorted(cycle, options) == std::vector<std::size_t>{2, 1, 0},
            "Non-transitive priority ordering did not follow source selection-and-move");
  }
  options = {};
  options.mode = EdictSortMode::kLength;
  const std::vector<EdictRecord> length_cycle = {record(U"zz", U"r"), record(U"a", U"r"), record(U"m", U"s")};
  require(sorted(length_cycle, options) == std::vector<std::size_t>{2, 0, 1},
          "Non-transitive length ordering did not follow source selection-and-move");
}

void test_identity_unicode_and_bounds() {
  std::vector<EdictRecord> duplicates(4, record(U"word", U"reading", U"definition"));
  for (std::size_t i = 0; i < duplicates.size(); ++i) {
    duplicates[i].byte_offset = i * 20;
    duplicates[i].byte_length = 20 + i;
  }
  require(sorted(duplicates) == std::vector<std::size_t>{0},
          "Deduplication skipped adjacent duplicates or retained the wrong provenance");
  duplicates[1].definitions = {U"a", U"b"};
  duplicates[2].definitions = {U"a, b"};
  require(sorted(duplicates).size() == 3, "Joined display text erased distinct definition structure");
  duplicates[3].readings = {U"r", U"s"};
  require(sorted(duplicates).size() == 4, "Deduplication erased distinct readings");

  EdictSortOptions options;
  options.mode = EdictSortMode::kEntry;
  const std::vector<EdictRecord> unicode = {record(U"\U0001f601", {}),
      record(U"\U0001f600", {}), record(U"\u3042", {}), record(U"A", {})};
  const auto before = unicode;
  require(sorted(unicode, options) == std::vector<std::size_t>{3, 2, 1, 0} && unicode == before,
          "Unicode extension lost scalars, original records or repertoire ordering");
  for (const auto page : {LegacyCodePage::k1250, LegacyCodePage::k1251,
                          LegacyCodePage::k1252, LegacyCodePage::k1253}) {
    for (const char32_t character : {U'A', U'\u3042', U'\u0391', U'\u0401'}) {
      const auto code = unicode_to_jwp_code(character, page);
      require(code && encode_jwp_text(std::u32string(1, character), page) == JwpText{*code},
              "Reusable sort mapping disagrees with the existing JWP codec");
    }
  }
  require(!unicode_to_jwp_code(U'\0') && !unicode_to_jwp_code(U'\u201a') &&
              !unicode_to_jwp_code(U'\U0001f600'), "JWP mapping silently substituted unsupported characters");

  const std::vector<EdictRecord> one = {record(U"a", {}, U"x")};
  EdictSortLimits limits;
  limits.text_cells = 3;
  require(sorted(one, {}, limits) == std::vector<std::size_t>{0}, "Exact prepared-text budget failed");
  limits.text_cells = 2;
  reject(one, {}, limits);
  limits = {};
  limits.records = 0;
  reject(one, {}, limits);
  limits = {};
  limits.comparisons = 0;
  reject({one.front(), one.front()}, {}, limits);
  limits = {};
  limits.work = 0;
  reject(one, {}, limits);
  require(sorted({}, {}, limits).empty(), "Empty sorting consumed work or invented results");
  options.mode = static_cast<EdictSortMode>(99);
  reject(one, options, {});
  options = {};
  options.code_page = static_cast<LegacyCodePage>(0);
  reject(one, options, {});
  for (const char32_t bad : {char32_t{0}, char32_t{0xd800}, char32_t{0x110000}}) {
    reject({record(std::u32string(1, bad), {})}, {}, {});
    reject({record(U"a", std::u32string(1, bad))}, {}, {});
    reject({record(U"a", {}, std::u32string(1, bad))}, {}, {});
  }
}

}  // namespace

int main() {
  try {
    test_reading_and_entry();
    test_length_and_cycles();
    test_identity_unicode_and_bounds();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
