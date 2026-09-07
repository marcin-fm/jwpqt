// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/romaji_conversion.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/wnn_session.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace jwpqt::core;

void require(bool condition, std::string_view message) {
  if (!condition) throw std::runtime_error(std::string(message));
}

template <typename Function>
void reject(Function&& function) {
  try {
    function();
  } catch (const KanaInputError&) {
    return;
  }
  throw std::runtime_error("Invalid romaji replay was accepted");
}

WnnDictionary dictionary(bool expanded = false) {
  const std::string candidate = expanded ? std::string("\xb0\xa1\xb0\xa1\xb0\xa1\xb0\xa1", 8)
                                          : std::string("\xb0\xa1/\xb0\xa2", 5);
  const std::string first = std::string("\xa2*", 2) + candidate + '\n';
  const std::string data = first + std::string("\xa2\xa4*\xb0\xa3\n", 6);
  std::string index("\xa2\x80\x80w\0\0\0\0\xa2\xa4\x80w", 12);
  const auto offset = static_cast<std::uint32_t>(first.size());
  for (unsigned shift = 0; shift < 32; shift += 8) {
    index.push_back(static_cast<char>((offset >> shift) & 0xffU));
  }
  return WnnDictionary::parse(index, data);
}

void test_replay() {
  const auto check = [](std::string_view input, std::u32string_view expected) {
    require(decode_jwp_text(convert_romaji_text(input)) == expected,
            "Romaji replay did not preserve recovered kana/case/punctuation behavior");
  };
  check("nihon", U"\u306b\u307b\u3093");
  check("Nihon", U"\u306b\u307b\u3093");
  check("NIHON", U"\u30cb\u30db\u30f3");
  check("kya kitte", U"\u304d\u3083\u3000\u304d\u3063\u3066");
  check("kan'i", U"\u304b\u3093\u3044");
  check("m'", U"\u3093");
  check("n\tA", U"\u3093\t\u30a2");
  check("a,.1x", U"\u3042\u3001\u3002\uff11\u00d7");

  for (JisCode code = 0x2421; code <= 0x2473; ++code) {
    const auto spelling = romaji_for_kana(code);
    require(spelling.has_value(), "Recovered kana spelling is missing");
    require(convert_romaji_text(*spelling) == JwpText{code},
            "A recovered hiragana spelling failed strict replay");
    std::string uppercase(*spelling);
    for (char& value : uppercase) {
      if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 'a' + 'A');
    }
    require(convert_romaji_text(uppercase) == JwpText{static_cast<JisCode>(code + 0x100)},
            "A recovered katakana spelling failed strict replay");
  }

  for (const std::string_view invalid : {"", "k", "ka k", "k!", "kn", "k\t", "a\nb",
                                         "a\rb", "\x7f", "\x80"}) {
    reject([&] { convert_romaji_text(invalid); });
  }
  reject([] { convert_romaji_text(std::string_view("a\0b", 3)); });
  reject([] { convert_romaji_text("a", nullptr, {}, 0); });
  reject([] { convert_romaji_text("a", nullptr, {}, std::numeric_limits<std::size_t>::max()); });
  require(convert_romaji_text(std::string(65535, '.')).size() == 65535,
          "Exact romaji selection limit was rejected");
  reject([] { convert_romaji_text(std::string(65536, '.')); });

  KanaInputComposer typing;
  typing.push_ascii('k');
  reject([&] { typing.set_options({false, true}); });
  require(!typing.push_ascii('!').empty(), "Strict replay changed normal typing defaults");
  KanaInputComposer strict({false, true});
  strict.push_ascii('k');
  reject([&] { strict.push_ascii('!'); });
  require(strict.pending(), "Rejected incomplete input discarded its prefix");
  require(!strict.push_ascii('a').empty(), "Rejected prefix could not be completed");
}

void test_automatic_spans() {
  const auto system = dictionary();
  WnnPreferences preferences(4);
  WnnConversionSession session(system, preferences);
  auto waiting = session.prepare_automatic({0x2422});
  require(waiting.wait_for_more && !waiting.conversion,
          "Unforced automatic conversion stopped waiting for more kana");
  auto forced = session.prepare_automatic({0x2422}, true);
  require(!forced.wait_for_more && forced.conversion && forced.matched_length == 1 &&
              forced.conversion->selected_candidate().text == JwpText{0x3021},
          "Forced automatic preparation did not retain the available candidate");

  require(convert_romaji_text("a", &session) == JwpText{0x2422} &&
              convert_romaji_text("A", &session) == JwpText{0x2522},
          "Replay automatically converted text without a kana-start event");
  require(convert_romaji_text("Ai", &session) == JwpText{0x3023},
          "A capitalized span did not wait for its complete key");
  require(convert_romaji_text("Aka", &session) == JwpText({0x3021, 0x242b}),
          "An automatic prefix discarded following kana");
  require(convert_romaji_text("Ai Ai", &session) == JwpText({0x3023, 0x2121, 0x3023}),
          "Multiple automatic spans did not preserve punctuation boundaries");
  require(convert_romaji_text("Ai\tAi", &session) == JwpText({0x3023, '\t', 0x3023}),
          "A tab did not end an automatic span");
  require(!session.active() && !preferences.changed(),
          "Batch preparation changed session state or preferences");

  require(session.begin({0x2422}) && session.select(1), "Could not prepare an existing WNN session");
  preferences.mark_saved();
  const auto saved = preferences.serialize();
  const auto generation = session.generation();
  require(convert_romaji_text("Aka", &session) == JwpText({0x3022, 0x242b}),
          "Replay ignored a learned candidate");
  reject([&] { convert_romaji_text("Aka k!", &session); });
  require(session.active() && session.generation() == generation && session.selected_index() == 1 &&
              session.input() == JwpText{0x2422} && !preferences.changed() && preferences.serialize() == saved,
          "Successful or failed replay stole the active session or preferences");

  const auto large = dictionary(true);
  WnnPreferences separate(4);
  WnnConversionSession expanded(large, separate);
  reject([&] { convert_romaji_text("Aka", &expanded, {}, 3); });
  require(!expanded.active() && !separate.changed(), "Output-limit failure changed WNN state");
}

}  // namespace

int main() {
  try {
    test_replay();
    test_automatic_spans();
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
