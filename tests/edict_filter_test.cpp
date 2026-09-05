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

}  // namespace

int main() {
  try {
    test_no_filter_preserves_record();
    test_personal_names_are_removed_or_stripped();
    test_places_are_removed_or_stripped();
    test_combined_filter_rejects_empty_records();
    test_personal_aliases_and_tag_boundaries();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
