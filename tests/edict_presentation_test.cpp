// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/edict_presentation.h"
#include "jwpqt/core/jwp_text_codec.h"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace jwpqt::core;

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

std::string describe(const std::vector<EdictPresentationItem>& items) {
  std::string result;
  for (const auto& item : items) {
    switch (item.kind) {
      case EdictPresentationKind::kEntry: result += std::to_string(item.index); break;
      case EdictPresentationKind::kPriorityEnd: result += '|'; break;
      case EdictPresentationKind::kContingent: result += 'C'; break;
      case EdictPresentationKind::kAdaptive: result += 'A'; break;
    }
  }
  return result;
}

void test_plan() {
  const std::vector<bool> flags{false, true, false, true};
  const std::vector<EdictSearchSection> phases{
      {EdictSearchStage::kDirect, 0, true}, {EdictSearchStage::kAdaptive, 2}};
  EdictPresentationOptions options;
  require(describe(prepare_edict_presentation(flags, phases, options)) == "1|0A3|2",
          "Marked phases lost stable priority order");
  options.advanced_separator = false;
  require(describe(prepare_edict_presentation(flags, phases, options)) == "1|302",
          "Unmarked adaptive priorities crossed the earlier marker incorrectly");
  options.priority_separator = false;
  require(describe(prepare_edict_presentation(flags, phases, options)) == "1302",
          "Unmarked phases did not share the insertion boundary");
  options.priority_first = false;
  require(describe(prepare_edict_presentation(flags, phases, options)) == "0123",
          "Disabled priority changed result order");
  options.advanced_separator = true;
  require(describe(prepare_edict_presentation(flags, phases, options)) == "01A23",
          "Advanced heading depended on priority grouping");
  require(describe(prepare_edict_presentation(flags, {}, {})) == "13|02",
          "Unspecified sections did not describe one direct pass");
  const std::vector<EdictSearchSection> names{
      {EdictSearchStage::kDirect, 0, true}, {EdictSearchStage::kDirect, 2, true}};
  require(describe(prepare_edict_presentation(flags, names)) == "1|03|2",
          "Separate resource passes shared priority boundaries");
  const std::vector<EdictSearchSection> retry{
      {EdictSearchStage::kDirect, 0, true}, {EdictSearchStage::kContingent, 0},
      {EdictSearchStage::kAdaptive, 2}};
  require(describe(prepare_edict_presentation(flags, retry)) == "C1|0A3|2",
          "Contingent results lost their own heading and boundary");
  auto empty = retry;
  empty[2].begin = 0;
  empty[1].quiet_empty = true;
  require(describe(prepare_edict_presentation(flags, empty)) == "A13|02",
          "Quiet fruitless contingent heading was retained");
  empty[1].quiet_empty = false;
  require(describe(prepare_edict_presentation({}, empty)) == "CA",
          "Empty attempted stages were silently lost");
  require(describe(prepare_edict_presentation({true, true}, {})) == "01" &&
          describe(prepare_edict_presentation({false, false}, {})) == "01",
          "Unnecessary priority separator was added");
  for (const auto& invalid : std::vector<std::vector<EdictSearchSection>>{
           {{EdictSearchStage::kDirect, 1, true}},
           {{EdictSearchStage::kDirect, 0, true}, {EdictSearchStage::kAdaptive, 5}},
           {{EdictSearchStage::kDirect, 0, true}, {EdictSearchStage::kDirect, 0}},
           {{static_cast<EdictSearchStage>(99), 0, true}},
           {{EdictSearchStage::kAdaptive, 0, true}},
           {{EdictSearchStage::kDirect, 0, true, true}}}) {
    bool rejected = false;
    try { (void)prepare_edict_presentation(flags, invalid); }
    catch (const EdictSearchError&) { rejected = true; }
    require(rejected, "Malformed section metadata was accepted");
  }
  require(prepare_edict_presentation(std::vector<bool>(100000, true), {}).size() == 100000,
          "Exact result budget failed");
  bool rejected = false;
  try { (void)prepare_edict_presentation(std::vector<bool>(100001), {}); }
  catch (const EdictSearchError&) { rejected = true; }
  require(rejected, "Result budget was not enforced");
  rejected = false;
  try { (void)prepare_edict_presentation({}, std::vector<EdictSearchSection>(4097)); }
  catch (const EdictSearchError&) { rejected = true; }
  require(rejected, "Section budget was not enforced");
}

void test_search_metadata() {
  const auto dictionary = EdictDictionary::parse(
      "cat /first/(P)/\ncat /second/(P)/(vulg)/\ncat /third/(P) elsewhere/\n", EdictEncoding::kUtf8);
  EdictSearchOptions options;
  options.name_filter.category_exclusions = 1U << 4;
  const auto found = search_edict_linear(dictionary, prepare_edict_query({'c','a','t'}), options);
  require(found.results.size() == 3 && found.results[0].priority &&
          !found.results[1].priority && !found.results[2].priority &&
          found.results[1].record.definitions.back() == U"(P)",
          "Priority was inferred from filtered rather than original definitions");
  require(found.sections.size() == 1 && found.sections[0].begin == 0 && found.sections[0].new_pass,
          "Direct search omitted section metadata");
  const auto legacy = EdictDictionary::parse("cat /first/(P)\ncat /second/(P)/\r\n", EdictEncoding::kEucJp);
  const auto legacy_found = search_edict_linear(legacy, prepare_edict_query({'c','a','t'}));
  require(legacy_found.results.size() == 2 && !legacy_found.results[0].priority &&
          legacy_found.results[1].priority, "Missing legacy slash was treated as a raw terminal priority marker");
  options.direct.require_beginning = options.direct.require_end = true;
  options.adaptive = options.contingent.enabled = true;
  const auto retry = search_edict_linear(dictionary, prepare_edict_query(encode_jwp_text(U"\u3042\u3044\u3046\u305f")), options);
  require(retry.results.empty() && retry.sections.size() == 3 &&
          retry.sections[1].stage == EdictSearchStage::kContingent && retry.sections[1].quiet_empty &&
          retry.sections[2].stage == EdictSearchStage::kAdaptive && retry.sections[2].begin == 0,
          "Empty fallback attempts lost source presentation metadata");
}
}  // namespace

int main() {
  try { test_plan(); test_search_metadata(); }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
