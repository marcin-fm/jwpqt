// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_index.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/legacy_text.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

template <typename Function>
void require_throws(Function&& function, const char* message) {
  try {
    function();
  } catch (const jwpqt::core::EdictIndexError&) {
    return;
  }
  require(false, message);
}

std::string index_bytes(std::uint32_t extent,
                        const std::vector<std::uint32_t>& offsets) {
  jwpqt::core::ByteWriter writer;
  writer.write_u32_le(extent);
  for (const std::uint32_t offset : offsets) {
    writer.write_u32_le(offset);
  }
  return writer.take_bytes();
}

void test_euc_index_round_trip_and_query() {
  using namespace jwpqt::core;
  const std::string source =
      "Alpha beta /first/\nalpha gamma /second/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const std::string bytes =
      index_bytes(static_cast<std::uint32_t>(source.size() + 15),
                  {1, 20, 7, 26});
  const EdictIndex index = EdictIndex::parse(bytes, dictionary);

  require(index.source_extent() == source.size() + 15 &&
              index.serialize() == bytes && index.entries().size() == 4,
          "EUC JDX index did not preserve its exact wire representation");
  require(index.entries()[1].record_index == 1 &&
              index.entries()[2].byte_offset == 6,
          "EUC JDX offsets did not map to their source records");

  const std::vector<EdictIndexEntry> alpha = index.find({'A', 'L', 'P'});
  require(alpha == std::vector<EdictIndexEntry>({{0, 0}, {19, 1}}),
          "EUC JDX query did not fold ASCII or return its equal range");
  require(index.find({'b', 'e', 't', 'a'}) ==
              std::vector<EdictIndexEntry>({{6, 0}}),
          "EUC JDX query did not find an interior dictionary word");
  require(index.find({'z', 'e', 't', 'a'}).empty(),
          "EUC JDX query reported a missing key");

  const EdictIndex unsorted = EdictIndex::parse(
      index_bytes(static_cast<std::uint32_t>(source.size()),
                  {7, 26, 1, 20}),
      dictionary);
  require(unsorted.find({'a', 'l', 'p'}) ==
              std::vector<EdictIndexEntry>({{0, 0}, {19, 1}}),
          "JDX lookup relied on unvalidated physical entry order");
}

void test_kana_normalization() {
  using namespace jwpqt::core;
  const std::string katakana = encode_legacy_text(
      U"\u30ab\u30ca /kana/\n", LegacyEncoding::kEucJp);
  const EdictDictionary dictionary =
      EdictDictionary::parse(katakana, EdictEncoding::kEucJp);
  const EdictIndex index = EdictIndex::parse(
      index_bytes(static_cast<std::uint32_t>(katakana.size()), {1}),
      dictionary);
  require(index.find({0x242b, 0x244a}) ==
              std::vector<EdictIndexEntry>({{0, 0}}),
          "EUC JDX query did not fold katakana to hiragana");
}

void test_utf8_index() {
  using namespace jwpqt::core;
  const std::string source = "\xef\xbb\xbfWord /first/\nword /second/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kUtf8);
  const EdictIndex index = EdictIndex::parse(
      index_bytes(static_cast<std::uint32_t>(source.size() + 15), {4, 17}),
      dictionary);
  require(index.find({'w', 'o', 'r', 'd'}).size() == 2,
          "UTF-8 JDX query did not preserve BOM-relative offsets");

  const std::string cyrillic_source = "\xd0\x81 /letter/\n";
  const EdictDictionary cyrillic_dictionary =
      EdictDictionary::parse(cyrillic_source, EdictEncoding::kUtf8);
  EdictIndexOptions options;
  options.utf8_code_page = LegacyCodePage::k1251;
  const EdictIndex cyrillic_index = EdictIndex::parse(
      index_bytes(static_cast<std::uint32_t>(cyrillic_source.size()), {1}),
      cyrillic_dictionary, options);
  const auto cyrillic_jis = unicode_to_jis_x0208(U'\u0401');
  require(cyrillic_jis.has_value() &&
              cyrillic_index.find({*cyrillic_jis}) ==
              std::vector<EdictIndexEntry>({{0, 0}}),
          "UTF-8 JDX lookup did not prefer recovered JIS Cyrillic mapping");
}

void test_mixed_index() {
  using namespace jwpqt::core;
  const std::string prefix =
      encode_legacy_text(U"日本", LegacyEncoding::kEucJp);
  const std::string source = prefix + " /word \xc0\xc1 \x8f\xa1/\n";
  const EdictDictionary dictionary = EdictDictionary::parse(
      source, EdictEncoding::kMixed, EdictParseLimits{},
      LegacyCodePage::k1251);
  const std::size_t word = source.find("word");
  const std::size_t extended = source.find("\xc0\xc1");
  const std::size_t page_byte = source.find("\x8f\xa1");
  const EdictIndex index = EdictIndex::parse(
      index_bytes(static_cast<std::uint32_t>(source.size()),
                  {1, static_cast<std::uint32_t>(word + 1),
                   static_cast<std::uint32_t>(extended + 1),
                   static_cast<std::uint32_t>(page_byte + 1)}),
      dictionary);

  require(index.find({0x467c, 0x4b5c}) ==
                  std::vector<EdictIndexEntry>{{0, 0}} &&
              index.find({'w', 'o', 'r', 'd'}) ==
                  std::vector<EdictIndexEntry>{{word, 0}} &&
              index.find_matches({0x4041}) ==
                  std::vector<EdictIndexMatch>{{extended, 2, 0}} &&
              index.find_matches({0x0f21}) ==
                  std::vector<EdictIndexMatch>{{page_byte, 2, 0}} &&
              index.find({0xc1}).empty(),
          "Mixed JDX lookup did not preserve recovered non-UTF byte stepping");
  require_throws(
      [&] {
        EdictIndex::parse(
            index_bytes(static_cast<std::uint32_t>(source.size()),
                        {static_cast<std::uint32_t>(extended + 2)}),
            dictionary);
      },
      "Mixed JDX accepted an offset inside a high-bit pair");
  require_throws(
      [&] {
        EdictIndex::parse(
            index_bytes(static_cast<std::uint32_t>(source.size()),
                        {static_cast<std::uint32_t>(page_byte + 2)}),
            dictionary);
      },
      "Mixed JDX treated the byte after 0x8f as a character start");
}

void test_malformed_indexes() {
  using namespace jwpqt::core;
  const std::string source = "word /definition/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const std::uint32_t size = static_cast<std::uint32_t>(source.size());

  require_throws([&] { EdictIndex::parse({}, dictionary); },
                 "Empty JDX index was accepted");
  require_throws(
      [&] { EdictIndex::parse(std::string(9, '\0'), dictionary); },
      "Misaligned JDX index was accepted");
  require_throws(
      [&] { EdictIndex::parse(index_bytes(size - 1, {1}), dictionary); },
      "JDX index with a short source header was accepted");
  require_throws(
      [&] { EdictIndex::parse(index_bytes(size + 16, {1}), dictionary); },
      "JDX index with an excessive source header was accepted");
  require_throws(
      [&] { EdictIndex::parse(index_bytes(size, {0}), dictionary); },
      "JDX index with a zero one-based offset was accepted");
  require_throws(
      [&] { EdictIndex::parse(index_bytes(size, {size + 1}), dictionary); },
      "JDX index with an out-of-range offset was accepted");
  const auto newline = static_cast<std::uint32_t>(source.size());
  require_throws(
      [&] { EdictIndex::parse(index_bytes(size, {newline}), dictionary); },
      "JDX index pointing at a line break was accepted");

  const EdictIndex header_only =
      EdictIndex::parse(index_bytes(size, {}), dictionary);
  require(header_only.entries().empty() &&
              header_only.find({'w'}).empty() &&
              header_only.serialize() == index_bytes(size, {}),
          "Valid header-only JDX index was not preserved");
  const EdictIndex end_checked =
      EdictIndex::parse(index_bytes(size, {1}), dictionary);
  require(end_checked.find({'w', 'o', 'r', 'd', ' ', '/', 'd', 'e', 'f',
                            'i', 'n', 'i', 't', 'i', 'o', 'n', '/', '\n',
                            'x'})
              .empty(),
          "JDX query past the dictionary end did not return a non-match");

  EdictIndexOptions options;
  options.encoded_bytes = 3;
  require_throws(
      [&] { EdictIndex::parse(index_bytes(size, {1}), dictionary, options); },
      "JDX encoded-size limit was not enforced");
  options.encoded_bytes = 8;
  options.entries = 0;
  require_throws(
      [&] { EdictIndex::parse(index_bytes(size, {1}), dictionary, options); },
      "JDX entry-count limit was not enforced");

  options.entries = 1;
  options.lookup_steps = 2;
  const EdictIndex work_limited =
      EdictIndex::parse(index_bytes(size, {1}), dictionary, options);
  require_throws([&] { work_limited.find({'w', 'o', 'r'}); },
                 "JDX lookup work limit was not enforced");

  options.lookup_steps = 100;
  options.matches = 0;
  const EdictIndex result_limited =
      EdictIndex::parse(index_bytes(size, {1}), dictionary, options);
  require_throws([&] { result_limited.find({'w'}); },
                 "JDX lookup result limit was not enforced");

  const std::string utf_source = "\xe6\x97\xa5 /sun/\n";
  const EdictDictionary utf_dictionary =
      EdictDictionary::parse(utf_source, EdictEncoding::kUtf8);
  require_throws(
      [&] {
        EdictIndex::parse(
            index_bytes(static_cast<std::uint32_t>(utf_source.size()), {2}),
            utf_dictionary);
      },
      "JDX index pointing into a UTF-8 continuation was accepted");
  require_throws([&] { EdictIndex::parse(index_bytes(size, {1}),
                                        dictionary)
                           .find({}); },
                 "Empty JDX query was accepted");
}

void test_euc_jis_x0212_match_span() {
  using namespace jwpqt::core;
  const std::string source = "\x8f\xa2\xed /copyright/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const EdictIndex index = EdictIndex::parse(
      index_bytes(static_cast<std::uint32_t>(source.size()), {1}),
      dictionary);
  require(index.find_matches({0xa9}) ==
              std::vector<EdictIndexMatch>{{0, 3, 0}},
          "JDX lookup did not map an EUC JIS X 0212 character or span");
}

}  // namespace

int main() {
  test_euc_index_round_trip_and_query();
  test_kana_normalization();
  test_utf8_index();
  test_mixed_index();
  test_malformed_indexes();
  test_euc_jis_x0212_match_span();
  return 0;
}
