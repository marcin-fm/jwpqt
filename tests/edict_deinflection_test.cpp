// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <utility>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/edict_deinflection.h"

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

template <typename Function>
void require_deinflection_throws(Function&& function,
                                 std::string_view message) {
  try {
    function();
  } catch (const jwpqt::core::EdictDeinflectionError&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

using Queries = std::vector<jwpqt::core::JwpText>;

void test_non_kana_and_ascii_queries() {
  using namespace jwpqt::core;
  require(generate_edict_deinflection_queries(
              prepare_edict_query({'c', 'a', 't'}))
              .empty(),
          "ASCII query unexpectedly generated deinflection variants");

  const Queries generated = generate_edict_deinflection_queries(
      prepare_edict_query({0x3021}));
  require(generated == Queries{{0x3021, 0x246b}, {0x3021, 0x2424}},
          "Non-kana ending variants did not match the recovered order");
}

void test_i_ending_and_truncation_order() {
  using namespace jwpqt::core;
  const Queries generated = generate_edict_deinflection_queries(
      prepare_edict_query({0x242b, 0x2424}));
  require(generated ==
              Queries{{0x242b, 0x242f}, {0x242b, 0x2430},
                      {0x242b, 0x2424, 0x246b}, {0x242b, 0x2426},
                      {0x242b, 0x2424, 0x2424}, {0x242b, 0x2424}},
          "I-ending adaptive variants or single-kana handling changed");

  const Queries longer = generate_edict_deinflection_queries(
      prepare_edict_query({0x3021, 0x242b, 0x2424}));
  require(longer.size() > 6 &&
              longer[5] == JwpText({0x3021, 0x242b}),
          "Truncated base query was not searched before its endings");
}

void test_special_endings() {
  using namespace jwpqt::core;
  const Queries sokuon = generate_edict_deinflection_queries(
      prepare_edict_query({0x242b, 0x2443}));
  require(sokuon == Queries{{0x242b, 0x2426}, {0x242b, 0x2444},
                            {0x242b, 0x246b}, {0x242b, 0x2424}},
          "Sokuon adaptive variants changed");

  const Queries nasal = generate_edict_deinflection_queries(
      prepare_edict_query({0x242b, 0x2473}));
  require(nasal == Queries{{0x242b, 0x244c}, {0x242b, 0x2456},
                           {0x242b, 0x2460}, {0x242b, 0x2424}},
          "Nasal adaptive variants changed");
}

void test_general_godan_and_option() {
  using namespace jwpqt::core;
  const Queries generated = generate_edict_deinflection_queries(
      prepare_edict_query({0x3021, 0x242b}));
  require(generated == Queries{{0x3021, 0x242f}, {0x3021, 0x242b, 0x2424},
                               {0x3021}, {0x3021, 0x246b},
                               {0x3021, 0x2424}},
          "General godan or truncation variants changed");

  EdictDeinflectionOptions options;
  options.include_i_adjectives = false;
  const Queries without_i = generate_edict_deinflection_queries(
      prepare_edict_query({0x3021, 0x246f}), options);
  require(without_i == Queries{{0x3021, 0x2426}, {0x3021},
                               {0x3021, 0x246b}},
          "Wa normalization or i-adjective toggle changed");

  const std::vector<std::pair<std::uint16_t, std::uint16_t>> godan_cases = {
      {0x2422, 0x2426}, {0x242b, 0x242f}, {0x242c, 0x2430},
      {0x2435, 0x2439}, {0x243f, 0x2444}, {0x244a, 0x244c},
      {0x2450, 0x2456}, {0x245e, 0x2460}, {0x2469, 0x246b},
  };
  for (const auto [source, ending] : godan_cases) {
    const Queries variants = generate_edict_deinflection_queries(
        prepare_edict_query({0x3021, source}));
    require(!variants.empty() && variants.front() == JwpText({0x3021, ending}),
            "Recovered godan ending map changed");
  }
}

void test_limits_and_public_state_validation() {
  using namespace jwpqt::core;
  JwpText maximum(100, 0x3021);
  const Queries generated = generate_edict_deinflection_queries(
      prepare_edict_query(maximum));
  require(!generated.empty() && generated.front().size() == 101,
          "Maximum input did not preserve the legacy 101-token candidate");

  EdictQuery forged;
  forged.key = {0x252b, 0x2524};
  forged.kind = EdictQueryKind::kAscii;
  const Queries normalized = generate_edict_deinflection_queries(forged);
  require(!normalized.empty() && normalized.front().front() == 0x242b,
          "Deinflection trusted forged query classification or skipped folding");

  EdictQuery high_bit_kana;
  high_bit_kana.key = {0xa52b, 0xa524};
  const Queries high_bit_generated =
      generate_edict_deinflection_queries(high_bit_kana);
  require(!high_bit_generated.empty() &&
              high_bit_generated.front() == JwpText({0x242b, 0x242f}),
          "High-bit kana were not canonicalized before deinflection");

  EdictQuery high_bit_hiragana;
  high_bit_hiragana.key = {0xa42b, 0xa424};
  require(generate_edict_deinflection_queries(high_bit_hiragana) ==
              normalized,
          "High-bit hiragana did not share canonical deinflection behavior");

  for (const std::uint16_t pattern :
       {std::uint16_t{0x2129}, std::uint16_t{0x214e},
        std::uint16_t{0x214f}, std::uint16_t{0x2174},
        std::uint16_t{0x2176}}) {
    require_deinflection_throws(
        [pattern] {
          generate_edict_deinflection_queries(
              prepare_edict_query({0x242b, pattern}));
        },
        "Pattern query was accepted by the non-pattern deinflection engine");
    require_deinflection_throws(
        [pattern] {
          generate_edict_deinflection_queries(
              prepare_edict_query(
                  {0x242b, static_cast<std::uint16_t>(pattern | 0x8080U)}));
        },
        "High-bit pattern query was accepted by the deinflection engine");
  }

  EdictDeinflectionOptions limited;
  limited.queries = 0;
  require_deinflection_throws(
      [&] {
        generate_edict_deinflection_queries(
            prepare_edict_query({0x3021}), limited);
      },
      "Zero deinflection query limit was not enforced");
  limited.queries = 1;
  require_deinflection_throws(
      [&] {
        generate_edict_deinflection_queries(
            prepare_edict_query({0x3021}), limited);
      },
      "One-under deinflection query limit was not enforced");
  limited.queries = 2;
  limited.work_steps = 3;
  require(generate_edict_deinflection_queries(
              prepare_edict_query({0x3021}), limited)
              .size() == 2,
          "Exact deinflection result/work budgets were rejected");
  limited.work_steps = 0;
  require_deinflection_throws(
      [&] {
        generate_edict_deinflection_queries(
            prepare_edict_query({0x3021}), limited);
      },
      "Zero deinflection work limit was not enforced");
  limited.work_steps = 2;
  require_deinflection_throws(
      [&] {
        generate_edict_deinflection_queries(
            prepare_edict_query({0x3021}), limited);
      },
      "One-under deinflection work limit was not enforced");
}

}  // namespace

int main() {
  test_non_kana_and_ascii_queries();
  test_i_ending_and_truncation_order();
  test_special_endings();
  test_general_godan_and_option();
  test_limits_and_public_state_validation();
  return 0;
}
