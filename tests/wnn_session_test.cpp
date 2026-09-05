// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_session.h"
#include "jwpqt/core/wnn_user_dictionary.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using jwpqt::core::JwpText;
using jwpqt::core::WnnConversionSession;
using jwpqt::core::WnnDictionary;
using jwpqt::core::WnnPreferences;
using jwpqt::core::WnnSessionError;
using jwpqt::core::WnnUserDictionary;
using jwpqt::core::WnnUserEntry;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Function>
void require_session_error(Function&& function, std::string_view message) {
  try {
    function();
  } catch (const WnnSessionError&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

void append_u32_le(std::string& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
  bytes.push_back(static_cast<char>((value >> 16U) & 0xffU));
  bytes.push_back(static_cast<char>((value >> 24U) & 0xffU));
}

WnnDictionary dictionary() {
  const std::string data = std::string("\xa2", 1) +
                           "*\xb0\xa1/\xb0\xa2\n";
  std::string index({static_cast<char>(0xa2), static_cast<char>(0x80),
                     static_cast<char>(0x80), 'w'});
  append_u32_le(index, 0);
  return WnnDictionary::parse(index, data);
}

WnnDictionary automatic_dictionary() {
  const std::string first = std::string("\xa2", 1) + "*\xb0\xa1\n";
  const std::string data = first + std::string("\xa2\xa4", 2) +
                           "*\xb0\xa2\n";
  std::string index({static_cast<char>(0xa2), static_cast<char>(0x80),
                     static_cast<char>(0x80), 'w'});
  append_u32_le(index, 0);
  index.append({static_cast<char>(0xa2), static_cast<char>(0xa4),
                static_cast<char>(0x80), 'w'});
  append_u32_le(index, static_cast<std::uint32_t>(first.size()));
  return WnnDictionary::parse(index, data);
}

JwpText input() { return JwpText{0x2422}; }

void test_begin_cycle_accept_and_cancel() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(3);
  WnnConversionSession session(system, preferences);

  require(session.begin(input()), "Session did not start for a valid key");
  require(session.active() && session.selected_index() == 0,
          "Session did not begin at the default candidate");
  require(session.selected_candidate().text == JwpText{0x3021},
          "Session exposed the wrong default candidate");

  require(session.cycle_next() && session.selected_index() == 1,
          "Next did not select the second candidate");
  require(preferences.changed(), "Cycling did not learn the candidate");
  require(session.accept() == JwpText{0x3022} && !session.active(),
          "Accept did not return the candidate and end the session");

  preferences.mark_saved();
  require(session.begin(input()) && session.selected_index() == 1,
          "A learned candidate was not restored at session start");
  require(session.cycle_next() && session.selected_candidate().original_kana,
          "Next did not reach the original-kana candidate");
  require(!preferences.changed(),
          "Selecting original kana unexpectedly changed preferences");
  require(session.cycle_next() && session.selected_index() == 0,
          "Next did not wrap to the first candidate");
  require(preferences.changed(),
          "Returning to a cached default did not update preferences");
  require(session.cancel() == input() && !session.active(),
          "Cancel did not return original input and end the session");
}

void test_previous_wrap_and_direct_selection() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  require(session.begin(input()), "Session did not start");
  require(session.cycle_previous() && session.selected_index() == 2,
          "Previous did not wrap to the final candidate");
  require(!preferences.changed(),
          "Wrapped original-kana selection changed preferences");
  require(session.select(1) && session.selected_index() == 1,
          "Direct selection did not select the requested candidate");
  require(!session.select(1), "Selecting the current candidate reported change");
  require_session_error([&] { session.select(3); },
                        "Out-of-range direct selection was accepted");
}

void test_stale_preference_is_repaired_on_begin() {
  const WnnDictionary system = dictionary();
  std::string bytes(2 * jwpqt::core::kWnnPreferenceRecordSize, '\0');
  bytes[0] = static_cast<char>(0xa2);
  bytes[6] = 99;
  WnnPreferences preferences = WnnPreferences::parse(bytes, 2);
  WnnConversionSession session(system, preferences);

  require(session.begin(input()) && session.selected_index() == 0,
          "A stale preference did not fall back to the first candidate");
  require(preferences.changed(),
          "Session start did not mark a repaired stale preference dirty");
  require(preferences.entries()[0].selected_offset == 0,
          "Session start did not repair a stale preference offset");
}

void test_prepare_has_no_state_or_preference_side_effects() {
  const WnnDictionary system = dictionary();
  std::string bytes(2 * jwpqt::core::kWnnPreferenceRecordSize, '\0');
  bytes[0] = static_cast<char>(0xa2);
  bytes[6] = 99;
  WnnPreferences preferences = WnnPreferences::parse(bytes, 2);
  WnnConversionSession session(system, preferences);

  auto prepared = session.prepare(input());
  require(prepared && prepared->selected_index() == 0 &&
              prepared->selected_candidate().text == JwpText{0x3021},
          "Prepared conversion did not expose the preferred candidate");
  require(!session.active() && !preferences.changed() &&
              preferences.entries()[0].selected_offset == 99,
          "Preparing conversion changed session or preferences");

  session.activate(std::move(*prepared));
  require(session.active() && preferences.changed() &&
              preferences.entries()[0].selected_offset == 0,
          "Activating conversion did not repair stale preference");

  auto second = session.prepare(input());
  auto moved = std::move(*second);
  require_session_error([&] { second->selected_candidate(); },
                        "Moved-from preparation exposed a candidate");
}

void test_inactive_and_no_match_behavior() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  require_session_error([&] { session.selected_index(); },
                        "Inactive session exposed a selection");
  require_session_error([&] { session.accept(); },
                        "Inactive session accepted a selection");

  require(session.begin(input()), "Session did not start before no-match test");
  require(!session.begin(JwpText{0x2423}),
          "Unknown key unexpectedly started a session");
  require(!session.active(), "No-match lookup did not clear the old session");
}

void test_lookup_failure_preserves_active_session() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);
  require(session.begin(input()), "Session did not start before failure test");
  const JwpText selected = session.selected_candidate().text;

  try {
    session.begin(JwpText{0x3021});
    throw std::runtime_error("Invalid lookup input was accepted");
  } catch (const jwpqt::core::WnnLookupError&) {
  }
  require(session.active() && session.selected_candidate().text == selected,
          "Failed lookup changed an active session");
}

void test_automatic_preparation_waits_and_uses_longest_prefix() {
  const WnnDictionary system = automatic_dictionary();
  WnnPreferences preferences(2);
  WnnConversionSession session(system, preferences);

  auto waiting = session.prepare_automatic(JwpText{0x2422});
  require(waiting.wait_for_more && waiting.matched_length == 1 &&
              !waiting.conversion && !session.active() &&
              !preferences.changed(),
          "Automatic preparation did not wait for a longer key");

  auto full = session.prepare_automatic(JwpText{0x2422, 0x2424});
  require(!full.wait_for_more && full.matched_length == 2 && full.conversion &&
              full.conversion->selected_candidate().text == JwpText{0x3022},
          "Automatic preparation did not convert a complete key");

  auto prefix = session.prepare_automatic(JwpText{0x2422, 0x2426});
  require(!prefix.wait_for_more && prefix.matched_length == 1 &&
              prefix.conversion &&
              prefix.conversion->selected_candidate().text == JwpText{0x3021},
          "Automatic preparation did not choose the longest valid prefix");

  auto none = session.prepare_automatic(JwpText{0x2428});
  require(!none.wait_for_more && none.matched_length == 0 &&
              !none.conversion && !session.active() &&
              !preferences.changed(),
          "Automatic no-match preparation changed session state");
  auto empty = session.prepare_automatic({});
  require(empty.matched_length == 0 && !empty.wait_for_more &&
              !empty.conversion,
          "Empty automatic preparation produced a match");

  JwpText overlong{0x2422, 0x2424};
  overlong.resize(jwpqt::core::kWnnMaximumKeySize + 1U, 0x2426);
  auto bounded_prefix = session.prepare_automatic(overlong);
  require(!bounded_prefix.wait_for_more &&
              bounded_prefix.matched_length == 2 &&
              bounded_prefix.conversion &&
              bounded_prefix.conversion->selected_candidate().text ==
                  JwpText{0x3022},
          "Overlong automatic input did not back off to a valid prefix");
}

void test_user_dictionary_records_are_owned_and_searched() {
  const WnnDictionary system = dictionary();
  WnnPreferences preferences(2);
  const WnnUserDictionary user_dictionary = WnnUserDictionary::from_entries({
      WnnUserEntry{{0x2422}, '*', {{0x3023}}},
      WnnUserEntry{{0x2422, 0x2424}, '*', {{0x3024}}},
  });
  std::vector<jwpqt::core::WnnRecord> user_records =
      user_dictionary.lookup_records();
  WnnConversionSession session(system, preferences, user_records);
  user_records.clear();

  const auto prepared = session.prepare(input());
  require(prepared && prepared->result().can_extend &&
              prepared->result().candidates.size() == 4 &&
              prepared->result().candidates[0].text == JwpText{0x3021} &&
              prepared->result().candidates[1].text == JwpText{0x3022} &&
              prepared->result().candidates[2].text == JwpText{0x3023} &&
              prepared->result().candidates[3].original_kana,
          "Session did not search its owned user dictionary after system data");

  auto automatic = session.prepare_automatic(input());
  require(automatic.wait_for_more && automatic.matched_length == 1 &&
              !automatic.conversion,
          "Automatic conversion ignored a longer user dictionary key");
}

void run_tests() {
  test_begin_cycle_accept_and_cancel();
  test_previous_wrap_and_direct_selection();
  test_stale_preference_is_repaired_on_begin();
  test_prepare_has_no_state_or_preference_side_effects();
  test_inactive_and_no_match_behavior();
  test_lookup_failure_preserves_active_session();
  test_automatic_preparation_waits_and_uses_longest_prefix();
  test_user_dictionary_records_are_owned_and_searched();
}

}  // namespace

int main() {
  try {
    run_tests();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "wnn session tests passed\n";
  return EXIT_SUCCESS;
}
