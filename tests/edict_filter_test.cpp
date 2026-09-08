// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "jwpqt/core/edict_filter.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

jwpqt::core::EdictNameFilterOptions options(bool personal, bool places) {
  jwpqt::core::EdictNameFilterOptions result;
  result.reject_personal_names = personal;
  result.reject_place_names = places;
  return result;
}

jwpqt::core::EdictRecord sample_record() {
  jwpqt::core::EdictRecord record;
  record.byte_offset = 17;
  record.byte_length = 99;
  record.headword = U"sample";
  record.readings = {U"reading"};
  record.definitions = {
      U"(s) surname",         U"(p) place",
      U"(s,p) shared",       U"(n,s) unknown first",
      U"(s,n) unknown last", U"(vulg,s) mixed",
      U"x(s) literal",       U"(fem,s) female surname",
      U"(u) unspecified",   U"ordinary",
  };
  return record;
}

void test_no_filter_preserves_record() {
  using namespace jwpqt::core;
  const EdictRecord record = sample_record();
  const std::optional<EdictRecord> filtered = filter_edict_name_types(record);
  require(filtered.has_value() && *filtered == record,
          "Disabled name filtering changed the record");
}

void test_personal_names_are_removed_or_stripped() {
  using namespace jwpqt::core;
  const EdictRecord record = sample_record();
  const std::optional<EdictRecord> filtered = filter_edict_name_types(
      record, options(true, false));
  require(filtered.has_value(), "Personal-name filtering rejected all results");
  require(filtered->byte_offset == record.byte_offset &&
              filtered->byte_length == record.byte_length &&
              filtered->headword == record.headword &&
              filtered->readings == record.readings,
          "Personal-name filtering changed record identity");
  require(filtered->definitions ==
              std::vector<std::u32string>(
                  {U"(p) place", U"(p) shared", U"(n,s) unknown first",
                   U"(n) unknown last", U"(vulg) mixed", U"x(s) literal",
                   U"(fem) female surname", U"ordinary"}),
          "Personal-name filtering did not reproduce definition semantics");
  require(record == sample_record(),
          "Personal-name filtering mutated the source record");
}

void test_places_are_removed_or_stripped() {
  using namespace jwpqt::core;
  const EdictRecord record = sample_record();
  const std::optional<EdictRecord> filtered = filter_edict_name_types(
      record, options(false, true));
  require(filtered.has_value(), "Place filtering rejected all results");
  require(filtered->definitions ==
              std::vector<std::u32string>(
                  {U"(s) surname", U"(s) shared", U"(n,s) unknown first",
                   U"(s,n) unknown last", U"(vulg,s) mixed", U"x(s) literal",
                   U"(fem,s) female surname", U"(u) unspecified",
                   U"ordinary"}),
          "Place filtering did not reproduce definition semantics");
  require(record == sample_record(), "Place filtering mutated the source record");
}

void test_combined_filter_rejects_empty_records() {
  using namespace jwpqt::core;
  EdictRecord record;
  record.definitions = {U"(s) surname", U"(p) place", U"(s,p) shared"};
  require(!filter_edict_name_types(record, options(true, true)).has_value(),
          "Combined filtering retained a record with no definitions");

  record.definitions = {U"(vulg,s,p) classified", U"ordinary"};
  const std::optional<EdictRecord> filtered = filter_edict_name_types(
      record, options(true, true));
  require(filtered.has_value() &&
              filtered->definitions ==
                  std::vector<std::u32string>({U"(vulg) classified",
                                               U"ordinary"}),
          "Combined filtering did not retain mixed non-name tags");
}

void test_personal_aliases_and_tag_boundaries() {
  using namespace jwpqt::core;
  EdictRecord aliases;
  aliases.definitions = {U"(g) given", U"(f) female", U"(m) male"};
  require(!filter_edict_name_types(aliases, options(true, false)).has_value(),
          "Personal-name aliases were not rejected");

  EdictRecord boundaries;
  boundaries.definitions = {
      U"word (s,p) space",     U"(vulg)(s,p) parenthesis",
      U"word,(s,p) comma",    U"word/(s,p) slash",
      U"word:(s,p) excluded", U"(vulg,s) (p,s) multiple",
      U"(s unterminated",     U"(n,s) unknown",
  };
  const std::optional<EdictRecord> filtered = filter_edict_name_types(
      boundaries, options(true, false));
  require(filtered.has_value() &&
              filtered->definitions ==
                  std::vector<std::u32string>(
                      {U"word (p) space", U"(vulg)(p) parenthesis",
                       U"word,(p) comma", U"word/(p) slash",
                       U"word:(s,p) excluded", U"(vulg) (p) multiple",
                       U"(s unterminated", U"(n,s) unknown"}),
          "Logical tag boundaries or malformed groups were handled incorrectly");
}

void test_all_category_exclusions() {
  using namespace jwpqt::core;
  std::uint32_t observed = 0;
  for (std::size_t i = 0; i < kEdictCategoryTags.size(); ++i) {
    const auto bit = std::uint32_t{1} << (i + 4);
    observed |= bit;
    const std::u32string tag(kEdictCategoryTags[i]);
    EdictNameFilterOptions filter;
    filter.category_exclusions = bit;
    auto record = sample_record();
    record.definitions = {U"(" + tag + U") rejected",
        U"(" + tag + U",s) surname", U"(s," + tag + U") surname",
        U"(n," + tag + U") unknown first", U"(" + tag + U",n) unknown last",
        U"literal(" + tag + U") untouched", U"(" + tag + U"X) not a prefix",
        U"(" + tag + U")", U"ordinary \ufeff\u00a0\U0001f600"};
    const auto before = record;
    const auto found = filter_edict_name_types(record, filter);
    require(found && found->definitions == std::vector<std::u32string>{
        U"(s) surname", U"(s) surname", U"(n," + tag + U") unknown first",
        U"(n) unknown last", U"literal(" + tag + U") untouched",
        U"(" + tag + U"X) not a prefix", U"ordinary \ufeff\u00a0\U0001f600"},
        "Category exclusion did not preserve the source's sense/group boundaries");
    require(record == before && found->headword == record.headword &&
        found->readings == record.readings && found->byte_offset == record.byte_offset &&
        found->byte_length == record.byte_length, "Category filtering changed provenance or input");
    for (std::size_t other = 0; other < kEdictCategoryTags.size(); ++other) {
      record.definitions = {U"(" + std::u32string(kEdictCategoryTags[other]) + U") sole"};
      require(filter_edict_name_types(record, filter).has_value() == (i != other),
              "A category exclusion affected another category");
    }
    record.definitions = {U"(" + tag + U",s,p) shared"};
    filter.reject_personal_names = true;
    auto shared = filter_edict_name_types(record, filter);
    require(shared && shared->definitions == std::vector<std::u32string>{U"(p) shared"},
            "Category and personal-name exclusions did not compose");
    filter.reject_place_names = true;
    require(!filter_edict_name_types(record, filter), "Combined filters retained an empty sense");
  }
  require(observed == kEdictCategoryMask, "Category mask differs from the 21 legacy bit positions");
  EdictNameFilterOptions all;
  all.category_exclusions = observed;
  auto record = sample_record();
  record.definitions.clear();
  for (const auto tag : kEdictCategoryTags)
    record.definitions.push_back(U"(" + std::u32string(tag) + U") sole");
  require(!filter_edict_name_types(record, all), "Combined category mask retained rejected senses");
  for (const auto invalid : {1U, 8U, 0x02000000U, 0x80000000U}) {
    all.category_exclusions = invalid;
    bool rejected = false;
    try { (void)filter_edict_name_types(record, all); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Unknown or name/boundary bits were accepted as category exclusions");
  }
}

}  // namespace

int main() {
  try {
    test_no_filter_preserves_record();
    test_personal_names_are_removed_or_stripped();
    test_places_are_removed_or_stripped();
    test_combined_filter_rejects_empty_records();
    test_personal_aliases_and_tag_boundaries();
    test_all_category_exclusions();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
