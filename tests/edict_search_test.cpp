// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/edict_dictionary.h"
#include "jwpqt/core/edict_index.h"
#include "jwpqt/core/edict_search.h"
#include "jwpqt/core/jwp_text_codec.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) throw std::runtime_error(std::string(message));
}

template <typename Function>
void require_throws(Function&& function, std::string_view message) {
  try {
    function();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

void append_u32_le(std::string& bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

std::string index_bytes(std::size_t source_size,
                        const std::vector<std::size_t>& offsets) {
  std::string bytes;
  append_u32_le(bytes, static_cast<std::uint32_t>(source_size));
  for (const std::size_t offset : offsets) {
    append_u32_le(bytes, static_cast<std::uint32_t>(offset + 1));
  }
  return bytes;
}

std::size_t offset_of(const std::string& source, std::string_view needle,
                      std::size_t from = 0) {
  const std::size_t offset = source.find(needle, from);
  require(offset != std::string::npos, "Synthetic EDICT term is missing");
  return offset;
}

void test_query_preparation() {
  using namespace jwpqt::core;
  require_throws([] { prepare_edict_query({}); },
                 "Empty EDICT query was accepted");
  require_throws([] { prepare_edict_query({'a', 'b'}); },
                 "Short ASCII EDICT query was accepted");
  require_throws([] { prepare_edict_query({'a', 'b', 0x2422}); },
                 "Mixed EDICT query was accepted");
  require_throws([] { prepare_edict_query({0xa9, 0x2422, 0x2424}); },
                 "Extended-byte/Japanese EDICT query was accepted");

  const EdictQuery ascii = prepare_edict_query({'C', 'A', 'T'});
  require(ascii.kind == EdictQueryKind::kAscii &&
              ascii.key == JwpText({'c', 'a', 't'}) && !ascii.truncated,
          "ASCII EDICT query was not normalized");
  const EdictQuery kana = prepare_edict_query({0x2522, 0x2424});
  require(kana.kind == EdictQueryKind::kJapanese &&
              kana.key == JwpText({0x2422, 0x2424}),
          "Katakana EDICT query was not folded to hiragana");
  const EdictQuery extended = prepare_edict_query({0xa9, 0xae, 0x99});
  require(extended.kind == EdictQueryKind::kAscii,
          "Legacy extended bytes were not classified as ASCII");

  JwpText long_query(101, static_cast<std::uint16_t>('a'));
  const EdictQuery truncated = prepare_edict_query(long_query);
  require(truncated.truncated && truncated.key.size() == 100,
          "Long EDICT query was not bounded like the legacy search box");
}

void test_ascii_boundaries_and_spans() {
  using namespace jwpqt::core;
  const std::string source =
      "cat /a cat/catfish/(tag) cat/\n"
      "bobcat /joined/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const std::size_t first = offset_of(source, "cat");
  const std::size_t second = offset_of(source, "cat", first + 1);
  const std::size_t third = offset_of(source, "cat", second + 1);
  const std::size_t fourth = offset_of(source, "cat", third + 1);
  const std::size_t fifth = offset_of(source, "cat", fourth + 1);
  const EdictIndex index = EdictIndex::parse(
      index_bytes(source.size(), {first, second, third, fourth, fifth}),
      dictionary);

  const EdictQuery query = prepare_edict_query({'C', 'A', 'T'});
  const std::vector<EdictIndexMatch> all =
      search_edict_direct(dictionary, index, query);
  require(all.size() == 5 && all[0].byte_offset == first &&
              all[0].byte_length == 3,
          "Direct ASCII search lost an indexed occurrence or byte span");

  EdictDirectSearchOptions closed;
  closed.require_beginning = true;
  closed.require_end = true;
  const std::vector<EdictIndexMatch> words =
      search_edict_direct(dictionary, index, query, closed);
  require(words.size() == 3 && words[0].byte_offset == first &&
              words[1].byte_offset == second &&
              words[2].byte_offset == fourth,
          "ASCII beginning/end search did not apply legacy boundaries");

  closed.full_ascii_boundaries = true;
  const std::vector<EdictIndexMatch> strict =
      search_edict_direct(dictionary, index, query, closed);
  require(strict.size() == 1 && strict[0].byte_offset == fourth,
          "Full-ASCII boundary mode did not require slash/tag boundaries");

  closed.results = 0;
  require_throws(
      [&] { search_edict_direct(dictionary, index, query, closed); },
      "Direct search result limit was not enforced");

  EdictQuery forged = query;
  forged.kind = EdictQueryKind::kJapanese;
  forged.key = {'C', 'A', 'T'};
  closed.results = 10;
  closed.full_ascii_boundaries = false;
  const std::vector<EdictIndexMatch> revalidated =
      search_edict_direct(dictionary, index, forged, closed);
  require(revalidated == words,
          "Direct search trusted forged query normalization or kind");
  forged.key = {'a', 'b'};
  require_throws(
      [&] { search_edict_direct(dictionary, index, forged, closed); },
      "Direct search trusted a forged short ASCII query");
}

void test_euc_jis_x0212_direct_search() {
  using namespace jwpqt::core;
  const std::string source = "\x8f\xa2\xed\x8f\xa2\xee\x8f\xa2\xef /marks/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kEucJp);
  const EdictIndex index = EdictIndex::parse(
      index_bytes(source.size(), {0}), dictionary);
  EdictDirectSearchOptions closed;
  closed.require_beginning = true;
  closed.require_end = true;
  const std::vector<EdictIndexMatch> matches = search_edict_direct(
      dictionary, index, prepare_edict_query({0xa9, 0xae, 0x99}), closed);
  require(matches == std::vector<EdictIndexMatch>{{0, 9, 0}},
          "Direct search did not match a recovered EUC JIS X 0212 term");
}

void test_japanese_boundaries_and_binding() {
  using namespace jwpqt::core;
  const std::string source =
      "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e "
      "[\xe3\x81\xab\xe3\x81\xbb\xe3\x82\x93\xe3\x81\x94] /Japanese/\n"
      "\xe6\x97\xa5\xe6\x9c\xac "
      "[\xe3\x81\xab\xe3\x81\xbb\xe3\x82\x93] /Japan/\n";
  const EdictDictionary dictionary =
      EdictDictionary::parse(source, EdictEncoding::kUtf8);
  const std::size_t compound = offset_of(source, "\xe6\x97\xa5\xe6\x9c\xac");
  const std::size_t exact =
      offset_of(source, "\xe6\x97\xa5\xe6\x9c\xac", compound + 1);
  const std::size_t reading = offset_of(
      source, "\xe3\x81\xab\xe3\x81\xbb\xe3\x82\x93", exact);
  const EdictIndex index = EdictIndex::parse(
      index_bytes(source.size(), {compound, exact, reading}), dictionary);

  EdictDirectSearchOptions closed;
  closed.require_beginning = true;
  closed.require_end = true;
  const EdictQuery kanji =
      prepare_edict_query(encode_jwp_text(U"\u65e5\u672c"));
  const std::vector<EdictIndexMatch> exact_result =
      search_edict_direct(dictionary, index, kanji, closed);
  require(exact_result.size() == 1 && exact_result[0].byte_offset == exact &&
              exact_result[0].byte_length == 6,
          "Japanese beginning/end search accepted a longer compound");

  const EdictQuery kana =
      prepare_edict_query(encode_jwp_text(U"\u306b\u307b\u3093"));
  const std::vector<EdictIndexMatch> reading_result =
      search_edict_direct(dictionary, index, kana, closed);
  require(reading_result.size() == 1 &&
              reading_result[0].byte_offset == reading,
          "Japanese bracket boundaries rejected an exact reading");

  const EdictDictionary other = EdictDictionary::parse(
      "\xe6\x97\xa5\xe6\x9c\xac /other/\n", EdictEncoding::kUtf8);
  require_throws(
      [&] { search_edict_direct(other, index, kanji, closed); },
      "Direct search accepted an index bound to another dictionary");
}

}  // namespace

int main() {
  test_query_preparation();
  test_ascii_boundaries_and_spans();
  test_euc_jis_x0212_direct_search();
  test_japanese_boundaries_and_binding();
  return 0;
}
