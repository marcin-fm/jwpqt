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

void test_lookup_stops_at_record_boundaries() {
  using namespace jwpqt::core;

  const auto require_no_cross_record_match = [](const std::string& source,
                                                 EdictEncoding encoding,
                                                 std::size_t offset,
                                                 const JwpText& query,
                                                 const char* message) {
    const EdictDictionary dictionary = EdictDictionary::parse(
        source, encoding, EdictParseLimits{}, LegacyCodePage::k1251);
    EdictIndexOptions options;
    options.lookup_steps = query.size();
    const EdictIndex index = EdictIndex::parse(
        index_bytes(static_cast<std::uint32_t>(source.size()),
                    {static_cast<std::uint32_t>(offset + 1)}),
        dictionary, options);
    const EdictIndexLookup lookup =
        index.find_matches_bounded(query, query.size(), 1);
    require(lookup.matches.empty() &&
                lookup.work_steps == query.size() - 1,
            message);
    require_throws(
        [&] {
          (void)index.find_matches_bounded(query, query.size() - 2, 1);
        },
        "Record-boundary lookup did not charge its failed comparison");
  };

  const std::string ascii = "a /x/\nb /y/\n";
  require_no_cross_record_match(ascii, EdictEncoding::kEucJp,
                                ascii.find('x'), {'x', '/', '\n', 'b'},
                                "ASCII JDX lookup crossed a record boundary");

  const std::string euc = "a /\xa4\xa2/\n\xa4\xa4 /y/\n";
  require_no_cross_record_match(
      euc, EdictEncoding::kEucJp, euc.find("\xa4\xa2"),
      {0x2422, '/', '\n', 0x2424},
      "EUC JDX lookup crossed a record boundary");

  const std::string utf8 = "a /\xe6\x97\xa5/\n\xe6\x9c\xac /y/\n";
  require_no_cross_record_match(
      utf8, EdictEncoding::kUtf8, utf8.find("\xe6\x97\xa5"),
      {0x467c, '/', '\n', 0x4b5c},
      "UTF-8 JDX lookup crossed a record boundary");

  const std::string mixed = "a /\xc0\xc1/\nb /y/\n";
  require_no_cross_record_match(
      mixed, EdictEncoding::kMixed, mixed.find("\xc0\xc1"),
      {0x4041, '/', '\n', 'b'},
      "Mixed JDX lookup crossed a record boundary");
}

void test_linear_lookup_preserves_source_order_and_occurrences() {
  using namespace jwpqt::core;
  const std::string source =
      "alpha alpha /first/\nbeta alpha /second/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const EdictIndexLookup lookup =
      find_edict_linear_matches(dictionary, {'A', 'L', 'P', 'H', 'A'});
  const std::size_t second = source.find("alpha", 1);
  const std::size_t third = source.find("alpha", second + 1);
  require(lookup.matches ==
              std::vector<EdictIndexMatch>{{0, 5, 0},
                                           {third, 5, 1}},
          "Linear lookup did not preserve one physical result per record");

  const EdictIndexLookup no_cross =
      find_edict_linear_matches(dictionary, {'t', '/', '\n', 'b'});
  require(no_cross.matches.empty(),
          "Linear lookup crossed an EDICT record boundary");
}

void test_linear_lookup_encodings() {
  using namespace jwpqt::core;
  const std::string euc = "\xa4\xab\xa5\xca\x8f\xa2\xed /entry/\n";
  const EdictDictionary euc_dictionary =
      EdictDictionary::parse(euc, EdictEncoding::kEucJp);
  require(find_edict_linear_matches(euc_dictionary,
                                    {0x242b, 0x244a, 0xa9})
                  .matches ==
              std::vector<EdictIndexMatch>{{0, 7, 0}},
          "Linear EUC lookup did not fold katakana or decode JIS X 0212");
  require(find_edict_linear_matches(euc_dictionary,
                                    {0xa4ab, 0xa5ca, 0xa9})
                  .matches ==
              std::vector<EdictIndexMatch>{{0, 7, 0}},
          "Linear lookup did not normalize raw high-bit kana query tokens");

  const std::string utf8 = "\xd0\x81 word /entry/\n";
  const EdictDictionary utf8_dictionary = EdictDictionary::parse(
      utf8, EdictEncoding::kUtf8, EdictParseLimits{},
      LegacyCodePage::k1251);
  const auto cyrillic = unicode_to_jis_x0208(U'\u0401');
  require(cyrillic.has_value() &&
              find_edict_linear_matches(utf8_dictionary, {*cyrillic})
                      .matches ==
                  std::vector<EdictIndexMatch>{{0, 2, 0}},
          "Linear UTF-8 lookup ignored the dictionary extension page");

  const std::string mixed = "head /\xc0\xc1 \x8f\xa1/\n";
  const EdictDictionary mixed_dictionary = EdictDictionary::parse(
      mixed, EdictEncoding::kMixed, EdictParseLimits{},
      LegacyCodePage::k1251);
  require(find_edict_linear_matches(mixed_dictionary, {0x4041}).matches ==
                  std::vector<EdictIndexMatch>{{6, 2, 0}} &&
              find_edict_linear_matches(mixed_dictionary, {0x0f21}).matches ==
                  std::vector<EdictIndexMatch>{{9, 2, 0}},
          "Linear mixed lookup did not preserve paired high-byte stepping");
}

void test_linear_lookup_limits_and_validation() {
  using namespace jwpqt::core;
  const std::string source = "word word /entry/\nword /second/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const EdictIndexLookup one =
      find_edict_linear_matches(dictionary, {'w', 'o', 'r', 'd'}, 100, 2);
  require(one.matches.size() == 2 && one.work_steps == 8,
          "Linear lookup did not report attempted comparison work");
  require_throws(
      [&] {
        (void)find_edict_linear_matches(dictionary, {'w', 'o', 'r', 'd'},
                                        one.work_steps - 1, 2);
      },
      "Linear lookup work limit was not enforced");
  require_throws(
      [&] { (void)find_edict_linear_matches(dictionary, {'w'}, 100, 1); },
      "Linear lookup result limit was not enforced");
  require_throws(
      [&] { (void)find_edict_linear_matches(dictionary, {}, 100, 1); },
      "Linear lookup accepted an empty key");
  require_throws(
      [&] { (void)find_edict_linear_matches(dictionary, {'w'}, 0, 1); },
      "Linear lookup accepted a zero work limit");
  require_throws(
      [&] { (void)find_edict_linear_matches(dictionary, {'w'}, 100, 0); },
      "Linear lookup accepted a zero result limit");
}

void test_linear_lookup_repeating_prefix_work() {
  using namespace jwpqt::core;
  std::u32string repeated(99, U'\u3042');
  repeated.push_back(U'\u3044');
  const std::string source =
      encode_legacy_text(repeated, LegacyEncoding::kEucJp) + " /entry/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  JwpText key(99, 0x2422);
  key.push_back(0x2426);
  const EdictIndexLookup lookup =
      find_edict_linear_matches(dictionary, key, 5'058, 1);
  require(lookup.matches.empty() && lookup.work_steps == 5'058,
          "Linear repeating-prefix lookup work accounting is wrong");
  require_throws(
      [&] {
        (void)find_edict_linear_matches(dictionary, key, 5'057, 1);
      },
      "Linear repeating-prefix lookup exceeded its work budget silently");
}

}  // namespace

int main() {
  test_euc_index_round_trip_and_query();
  test_kana_normalization();
  test_utf8_index();
  test_mixed_index();
  test_malformed_indexes();
  test_euc_jis_x0212_match_span();
  test_lookup_stops_at_record_boundaries();
  test_linear_lookup_preserves_source_order_and_occurrences();
  test_linear_lookup_encodings();
  test_linear_lookup_limits_and_validation();
  test_linear_lookup_repeating_prefix_work();
  return 0;
}
