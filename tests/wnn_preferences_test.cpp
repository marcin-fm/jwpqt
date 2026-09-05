// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_preferences.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using jwpqt::core::JwpText;
using jwpqt::core::WnnCandidate;
using jwpqt::core::WnnLookupResult;
using jwpqt::core::WnnPreferenceError;
using jwpqt::core::WnnPreferences;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Function>
void require_preference_error(Function&& function, std::string_view message) {
  try {
    function();
  } catch (const WnnPreferenceError&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

JwpText key(std::uint16_t cell) {
  return JwpText{static_cast<std::uint16_t>(0x2400U | cell)};
}

WnnLookupResult choices(const JwpText& original = key(0x22)) {
  return {{{JwpText{0x3021}, 0, false},
           {JwpText{0x3022}, 2, false},
           {original, 4, true}},
          false};
}

void test_learning_and_roundtrip() {
  WnnPreferences preferences(3);
  const JwpText input = key(0x22);
  const WnnLookupResult result = choices(input);
  require(preferences.preferred_candidate(input, result) == 0,
          "An empty cache did not select the first candidate");
  require(preferences.remember(input, result, 1),
          "A non-default choice was not learned");
  require(preferences.changed(), "Learning did not mark the cache changed");
  require(preferences.preferred_candidate(input, result) == 1,
          "The learned choice was not restored");

  const std::string bytes = preferences.serialize();
  require(bytes.size() == 3 * jwpqt::core::kWnnPreferenceRecordSize,
          "Preference serialization did not preserve fixed capacity");
  const auto restored = WnnPreferences::parse(bytes, 3);
  require(!restored.changed(), "Parsing marked the cache changed");
  require(restored.entries() == preferences.entries(),
          "Preference wire roundtrip changed entries");
  require(restored.preferred_candidate(input, result) == 1,
          "Roundtripped preference selected the wrong candidate");
}

void test_default_and_original_rules() {
  WnnPreferences preferences(2);
  const JwpText input = key(0x23);
  const WnnLookupResult result = choices(input);
  require(!preferences.remember(input, result, 0),
          "A new default choice was cached");
  require(!preferences.remember(input, result, 2),
          "The original-kana fallback was cached");
  require(!preferences.changed(), "Ignored choices changed the cache");

  require(preferences.remember(input, result, 1),
          "A non-default choice was not cached");
  preferences.mark_saved();
  require(preferences.remember(input, result, 0),
          "An existing preference was not reset to the default");
  require(preferences.preferred_candidate(input, result) == 0,
          "Reset preference did not select the first candidate");

  require(preferences.remember(input, result, 1),
          "Non-default choice was not relearned");
  WnnLookupResult deduplicated{{{input, 0, false}}, false};
  require(!preferences.remember(input, deduplicated, 0),
          "The final candidate was cached when its original marker was lost");
  require(preferences.preferred_candidate(input, result) == 1,
          "Selecting the final candidate changed an existing preference");
}

void test_recency_and_resize_match_legacy_table() {
  WnnPreferences preferences(2);
  const WnnLookupResult result = choices();
  require(preferences.remember(key(0x22), result, 1),
          "First preference was not learned");
  require(preferences.remember(key(0x23), result, 1),
          "Second preference was not learned");
  require(preferences.remember(key(0x24), result, 1),
          "Third preference was not learned");

  const std::array<std::uint8_t, 6> second_key{0xa3, 0, 0, 0, 0, 0};
  const std::array<std::uint8_t, 6> third_key{0xa4, 0, 0, 0, 0, 0};
  require(preferences.entries()[0].key == second_key &&
              preferences.entries()[1].key == third_key,
          "Fixed-capacity insertion did not shift out the oldest entry");

  preferences.resize(1);
  require(preferences.entries()[0].key == second_key,
          "Shrinking did not preserve the legacy table prefix");
  preferences.resize(3);
  require(preferences.entries()[0].key == second_key &&
              preferences.entries()[1].key[0] == 0 &&
              preferences.entries()[2].key[0] == 0,
          "Growing did not zero-fill new legacy table entries");
}

void test_wire_loading_and_stale_offsets() {
  std::string bytes;
  bytes.append({static_cast<char>(0xa2), 0, 0, 0, 0, 0, 99, 0});
  bytes.append({static_cast<char>(0xa3), 0, 0, 0, 0, 0,
                static_cast<char>(0xff), static_cast<char>(0xff)});
  const WnnPreferences preferences = WnnPreferences::parse(bytes, 2);
  require(preferences.preferred_candidate(key(0x22), choices()) == 0,
          "A stale candidate offset did not fall back to the first choice");
  require(preferences.preferred_candidate(key(0x23), choices()) == 0,
          "A negative candidate offset did not fall back safely");
  require(preferences.serialize() == bytes,
          "Opaque legacy preference entries did not roundtrip exactly");

  const WnnPreferences truncated = WnnPreferences::parse(bytes, 1);
  require(truncated.entries().size() == 1 &&
              truncated.entries()[0].key[0] == 0xa2,
          "Loading did not clamp complete records to capacity");

  const WnnPreferences partial_key =
      WnnPreferences::parse(std::string({static_cast<char>(0xa4),
                                         static_cast<char>(0xa5)}),
                            1);
  require(partial_key.entries()[0].key[0] == 0xa4 &&
              partial_key.entries()[0].key[1] == 0xa5 &&
              partial_key.entries()[0].key[2] == 0 &&
              partial_key.entries()[0].selected_offset == 0,
          "A partial record was not zero-filled like the legacy loader");

  std::string partial_offset(7, '\0');
  partial_offset[0] = static_cast<char>(0xa6);
  partial_offset[6] = 42;
  require(WnnPreferences::parse(partial_offset, 1)
              .entries()[0]
              .selected_offset == 42,
          "A partial selection offset was not zero-filled");

  std::string ignored_tail(9, '\0');
  ignored_tail[0] = static_cast<char>(0xa7);
  ignored_tail[8] = static_cast<char>(0xff);
  require(WnnPreferences::parse(ignored_tail, 1).serialize() ==
              ignored_tail.substr(0, 8),
          "Bytes beyond configured capacity were not ignored");
}

void test_validation_and_uncacheable_keys() {
  require_preference_error([] { WnnPreferences preferences(0); },
                           "Zero preference capacity was accepted");
  require_preference_error(
      [] {
        WnnPreferences preferences(
            jwpqt::core::kWnnMaximumPreferenceCapacity + 1);
      },
      "Oversized preference capacity was accepted");
  require_preference_error(
      [] { WnnPreferences preferences(std::numeric_limits<std::size_t>::max()); },
      "Huge preference capacity was not rejected before allocation");

  WnnPreferences preferences(2);
  WnnLookupResult result = choices();
  require_preference_error(
      [&] { preferences.remember(key(0x22), result, result.candidates.size()); },
      "An out-of-range candidate index was accepted");
  result.candidates[1].legacy_cell_offset =
      static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()) + 1;
  require_preference_error(
      [&] { preferences.remember(key(0x22), result, 1); },
      "An unrepresentable candidate offset was accepted");

  const JwpText long_key(7, 0x2422);
  require(!preferences.remember(long_key, choices(long_key), 1),
          "A key longer than six kana was cached");
  require(preferences.preferred_candidate(long_key, choices(long_key)) == 0,
          "An uncacheable key did not use the default candidate");
  require_preference_error(
      [&] { preferences.preferred_candidate(JwpText{0x3021}, choices()); },
      "A non-kana preference key was accepted");
}

void run_tests() {
  test_learning_and_roundtrip();
  test_default_and_original_rules();
  test_recency_and_resize_match_legacy_table();
  test_wire_loading_and_stale_offsets();
  test_validation_and_uncacheable_keys();
}

}  // namespace

int main() {
  try {
    run_tests();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "wnn preference tests passed\n";
  return EXIT_SUCCESS;
}
