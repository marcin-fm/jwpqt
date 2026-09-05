#include "jwpqt/core/kanji_color_list.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using jwpqt::core::JwpDocument;
using jwpqt::core::JwpParagraph;
using jwpqt::core::KanjiColorList;
using jwpqt::core::KanjiColorListError;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

template <typename Function>
void expect_error(Function function, std::string_view message) {
  try {
    function();
  } catch (const KanjiColorListError&) {
    return;
  }
  std::cerr << "FAIL: expected KanjiColorListError for " << message << '\n';
  std::exit(1);
}

std::string bytes(std::initializer_list<unsigned int> values) {
  std::string result;
  for (const unsigned int value : values) {
    result.push_back(static_cast<char>(value));
  }
  return result;
}

void test_membership_mutation_and_legacy_alias() {
  KanjiColorList list;
  expect(list.empty() && list.size() == 0, "new list was not empty");
  expect(list.add(0x3021) && !list.add(0x3021) && list.contains(0x3021),
         "list did not deduplicate a valid code");
  expect(!list.add(0x3020) && !list.contains(0x3020),
         "list accepted a code below its range");
  expect(list.add(0x307f) && list.contains(0x3121) && list.size() == 2,
         "list did not preserve the historical malformed-cell alias");
  expect(list.remove(0x3121) && !list.contains(0x307f) &&
             !list.remove(0x3121),
         "list alias removal did not address the shared bit");
  list.clear();
  expect(list.empty() && list.codes().empty(), "clear left list members");
}

void test_codec_canonicalizes_order_and_duplicates() {
  const std::string source = bytes({0xb1, 0xa1, 0xb0, 0xa1, 0xb1, 0xa1});
  const KanjiColorList list = KanjiColorList::parse(source);
  expect(list.size() == 2 && list.contains(0x3021) && list.contains(0x3121),
         "parser lost valid list members");
  expect(list.serialize() == bytes({0xb0, 0xa1, 0xb1, 0xa1}),
         "serializer did not produce sorted canonical EUC pairs");
  expect(KanjiColorList::parse(list.serialize()).codes() == list.codes(),
         "canonical color list did not roundtrip");
}

void test_codec_rejects_malformed_records() {
  expect_error([] { KanjiColorList::parse(bytes({0xb0})); },
               "truncated pair");
  expect_error([] { KanjiColorList::parse(bytes({0x30, 0xa1})); },
               "missing EUC high bit");
  expect_error([] { KanjiColorList::parse(bytes({0xb0, 0xff})); },
               "invalid EUC trail");
  expect_error([] { KanjiColorList::parse(bytes({0xa1, 0xa1})); },
               "code below list base");
  expect_error([] { KanjiColorList::parse(bytes({0xf4, 0xdc})); },
               "code beyond fixed capacity");
}

void test_document_scan_uses_raw_jwp_tokens() {
  JwpDocument document;
  JwpParagraph first;
  first.text = {'A', 0x2422, 0x3021, 0x5021, 0x3021};
  JwpParagraph page_break;
  page_break.page_break = true;
  JwpParagraph last;
  last.text = {0x745b, 0x745c, 0x2121};
  document.paragraphs = {first, page_break, last};

  KanjiColorList list;
  expect(list.add_document(document) == 3 && list.size() == 3,
         "document scan added the wrong raw JWP tokens");
  expect(list.add_document(document) == 0,
         "repeated document scan reported duplicate additions");
  expect(list.codes() ==
             std::vector<jwpqt::core::JisCode>({0x3021, 0x5021, 0x745b}),
         "document scan did not retain sorted index order");
}

}  // namespace

int main() {
  test_membership_mutation_and_legacy_alias();
  test_codec_canonicalizes_order_and_duplicates();
  test_codec_rejects_malformed_records();
  test_document_scan_uses_raw_jwp_tokens();
  std::cout << "kanji color list tests passed\n";
  return 0;
}
