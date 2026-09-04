// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_lookup.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using jwpqt::core::JisCode;
using jwpqt::core::JwpText;
using jwpqt::core::WnnCandidate;
using jwpqt::core::WnnDictionary;
using jwpqt::core::WnnLookupError;
using jwpqt::core::WnnRecord;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void append_u32_le(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

std::string encode_candidate(const JwpText& candidate) {
  std::string bytes;
  for (const JisCode code : candidate) {
    bytes.push_back(static_cast<char>(((code >> 8U) & 0x7fU) | 0x80U));
    bytes.push_back(static_cast<char>((code & 0x7fU) | 0x80U));
  }
  return bytes;
}

struct RecordSpec {
  std::vector<std::uint8_t> key;
  char ending = '*';
  std::vector<JwpText> candidates;
};

WnnDictionary dictionary(const std::vector<RecordSpec>& records) {
  std::string data;
  std::string index;
  std::optional<std::array<std::uint8_t, 3>> last_prefix;
  for (const RecordSpec& record : records) {
    const std::uint32_t offset = static_cast<std::uint32_t>(data.size());
    std::array<std::uint8_t, 3> prefix{0x80, 0x80, 0x80};
    for (std::size_t key_index = 0;
         key_index < record.key.size() && key_index < prefix.size();
         ++key_index) {
      prefix[key_index] = record.key[key_index];
    }
    if (!last_prefix || prefix != *last_prefix) {
      for (const std::uint8_t key_byte : prefix) {
        index.push_back(static_cast<char>(key_byte));
      }
      index.push_back('w');
      append_u32_le(index, offset);
      last_prefix = prefix;
    }

    for (const std::uint8_t key_byte : record.key) {
      data.push_back(static_cast<char>(key_byte));
    }
    data.push_back(record.ending);
    for (std::size_t candidate = 0; candidate < record.candidates.size();
         ++candidate) {
      if (candidate != 0) {
        data.push_back('/');
      }
      data += encode_candidate(record.candidates[candidate]);
    }
    data.push_back('\n');
  }
  return WnnDictionary::parse(index, data);
}

WnnRecord user_record(std::vector<std::uint8_t> key, char ending,
                      JwpText candidate) {
  return WnnRecord{0, std::move(key), ending, {std::move(candidate)}};
}

void test_pass_and_source_order() {
  const WnnDictionary system = dictionary({
      {{0xa2}, 'u', {{0x3023}}},
      {{0xa2, 0xa4}, '*', {{0x3021}}},
      {{0xa2, 0xa4}, '1', {{0x3022}}},
      {{0xa2, 0xa4}, 'i', {{0x3024}}},
  });
  const std::vector<WnnRecord> user{
      user_record({0xa2, 0xa4}, '1', {0x3122}),
      user_record({0xa2}, 'u', {0x3123}),
      user_record({0xa2, 0xa4}, '*', {0x3121}),
      user_record({0xa2, 0xa4}, 'i', {0x3124}),
  };
  const JwpText input{0x2422, 0x2424};
  const auto result =
      jwpqt::core::lookup_wnn_candidates(system, input, &user);
  const std::vector<JwpText> expected{
      {0x3021},         {0x3121},         {0x3022},
      {0x3122},         {0x3023, 0x2424}, {0x3123, 0x2424},
      input,
  };
  require(result.candidates.size() == expected.size(),
          "Lookup did not preserve pass/source ordering");
  for (std::size_t index = 0; index < expected.size(); ++index) {
    require(result.candidates[index].text == expected[index],
            "Lookup candidate order or suffix was wrong");
  }
  require(result.candidates.back().original_kana,
          "Original kana fallback was not marked");
  require(std::none_of(result.candidates.begin(), result.candidates.end(),
                       [](const WnnCandidate& candidate) {
                         return candidate.text == JwpText{0x3024} ||
                                candidate.text == JwpText{0x3124};
                       }),
          "Special-stem search did not stop after its first source match");
}

void test_script_folding_and_fallback() {
  const WnnDictionary system = dictionary({{{0xa2}, '*', {{0x3021}}}});
  const auto hiragana =
      jwpqt::core::lookup_wnn_candidates(system, JwpText{0x2422});
  const auto katakana =
      jwpqt::core::lookup_wnn_candidates(system, JwpText{0x2522});
  require(hiragana.candidates[0].text == katakana.candidates[0].text,
          "Kana scripts did not fold to the same WNN key");
  require(hiragana.candidates.back().text == JwpText{0x2422} &&
              katakana.candidates.back().text == JwpText{0x2522},
          "Original fallback did not preserve kana script");
}

void test_ending_rules() {
  const WnnDictionary system = dictionary({
      {{0xa2}, 'b', {{0x3021}}},
      {{0xa2}, 'r', {{0x3022}}},
      {{0xa2}, 'u', {{0x3023}}},
  });
  const auto n_result = jwpqt::core::lookup_wnn_candidates(
      system, JwpText{0x2422, 0x2473});
  require(n_result.candidates.front().text == JwpText({0x3021, 0x2473}),
          "Labial ending did not accept moraic n");

  const auto tsu_result = jwpqt::core::lookup_wnn_candidates(
      system, JwpText{0x2422, 0x2443});
  require(tsu_result.candidates[0].text == JwpText({0x3022, 0x2443}) &&
              tsu_result.candidates[1].text == JwpText({0x3023, 0x2443}),
          "r/u endings did not accept small tsu");

  const auto i_result = jwpqt::core::lookup_wnn_candidates(
      system, JwpText{0x2422, 0x2424});
  require(i_result.candidates[0].text == JwpText({0x3023, 0x2424}),
          "u ending did not fall through to the i rule");
}

void test_empty_candidates_receive_conjugated_suffix() {
  const WnnDictionary system = dictionary({
      {{0xa2}, 'u', {{0x3021}, {}}},
  });
  const JwpText input{0x2422, 0x2424};
  const auto result = jwpqt::core::lookup_wnn_candidates(system, input);
  require(result.candidates.size() == 3 &&
              result.candidates[0].text == JwpText({0x3021, 0x2424}) &&
              result.candidates[1].text == JwpText{0x2424} &&
              result.candidates[2].text == input,
          "Conjugation did not preserve a trailing empty candidate");
}

void test_deduplication_offsets_and_extension() {
  const JwpText input{0x2422};
  const WnnDictionary system = dictionary({
      {{0xa2}, '*', {input, {0x3021, 0x3022}, {0x3021, 0x3022}}},
      {{0xa2, 0xa4}, '*', {{0x3023}}},
  });
  const auto result = jwpqt::core::lookup_wnn_candidates(system, input);
  require(result.can_extend, "Longer dictionary key was not reported");
  require(result.candidates.size() == 2,
          "Stable duplicate filtering retained a duplicate");
  require(result.candidates[0] == WnnCandidate{input, 0, false},
          "Dictionary candidate equal to input did not win deduplication");
  require(result.candidates[1] ==
              WnnCandidate{JwpText{0x3021, 0x3022}, 2, false},
          "Legacy candidate cell offset was not recomputed after filtering");
}

void test_no_hit_and_validation() {
  const WnnDictionary system = dictionary({{{0xa2, 0xa4}, '*', {{0x3021}}}});
  const auto prefix =
      jwpqt::core::lookup_wnn_candidates(system, JwpText{0x2422});
  require(prefix.candidates.empty() && prefix.can_extend,
          "No-hit prefix result was incorrect");
  const auto empty = jwpqt::core::lookup_wnn_candidates(system, {});
  require(empty.candidates.empty() && empty.can_extend,
          "Empty lookup did not preserve legacy continuation state");

  const WnnDictionary stem_only =
      dictionary({{{0xa2}, '1', {{0x3021}}}});
  const auto stem =
      jwpqt::core::lookup_wnn_candidates(stem_only, JwpText{0x2422});
  require(stem.candidates.empty() && stem.can_extend,
          "Equal-key inflection record did not report continuation");

  const auto maximum_length = jwpqt::core::lookup_wnn_candidates(
      system, JwpText(jwpqt::core::kWnnMaximumKeySize, 0x2422));
  require(maximum_length.candidates.empty(),
          "Maximum-length lookup key was not accepted");

  bool invalid = false;
  try {
    (void)jwpqt::core::lookup_wnn_candidates(system, JwpText{0x3021});
  } catch (const WnnLookupError&) {
    invalid = true;
  }
  require(invalid, "Non-kana lookup input was accepted");

  bool too_long = false;
  try {
    (void)jwpqt::core::lookup_wnn_candidates(system,
                                             JwpText(20, 0x2422));
  } catch (const WnnLookupError&) {
    too_long = true;
  }
  require(too_long, "Overlong lookup input was accepted");

  bool limited = false;
  try {
    const WnnDictionary one =
        dictionary({{{0xa2}, '*', {{0x3021, 0x3022}}}});
    (void)jwpqt::core::lookup_wnn_candidates(one, JwpText{0x2422}, nullptr,
                                             3);
  } catch (const WnnLookupError&) {
    limited = true;
  }
  require(limited, "Candidate output budget was not enforced");

  const WnnDictionary exact_fit =
      dictionary({{{0xa2}, '*', {{0x3021, 0x3022}}}});
  const auto fitted = jwpqt::core::lookup_wnn_candidates(
      exact_fit, JwpText{0x2422}, nullptr, 4);
  require(fitted.candidates.size() == 2,
          "Exact output budget was rejected");

  std::vector<WnnRecord> hostile{
      user_record({0xa2}, '*', JwpText(5, 0x3021)),
  };
  bool hostile_limited = false;
  try {
    (void)jwpqt::core::lookup_wnn_candidates(exact_fit, JwpText{0x2422},
                                             &hostile, 4);
  } catch (const WnnLookupError&) {
    hostile_limited = true;
  }
  require(hostile_limited,
          "Oversized public user candidate bypassed the output budget");
}

}  // namespace

int main() {
  try {
    test_pass_and_source_order();
    test_script_folding_and_fallback();
    test_ending_rules();
    test_empty_candidates_receive_conjugated_suffix();
    test_deduplication_offsets_and_extension();
    test_no_hit_and_validation();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
