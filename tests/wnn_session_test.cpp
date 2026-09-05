// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_session.h"

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

void run_tests() {
  test_begin_cycle_accept_and_cancel();
  test_previous_wrap_and_direct_selection();
  test_stale_preference_is_repaired_on_begin();
  test_prepare_has_no_state_or_preference_side_effects();
  test_inactive_and_no_match_behavior();
  test_lookup_failure_preserves_active_session();
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
