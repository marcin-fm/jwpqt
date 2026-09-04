// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/kana_input.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string_view>

namespace jwpqt::core {
namespace {

constexpr std::size_t kBaseState = 0;
constexpr std::size_t kFState = 15;
constexpr std::size_t kNState = 17;
constexpr std::size_t kPlusState = 26;
constexpr std::size_t kYState = 36;
constexpr std::size_t kUpState = 41;
constexpr std::size_t kPendingState = 44;
constexpr std::size_t kDoneState = 100;
constexpr std::size_t kMaximumInput = 5;
constexpr JisCode kHiraganaBase = 0x2400;
constexpr JisCode kKatakanaBase = 0x2500;

struct KanaState {
  std::string_view valid;
  std::uint8_t length;
  std::size_t next;
};

// Desktop table from the recovered JWPxp 1.67 input implementation.
constexpr std::array<KanaState, 45> kStates{{
    {"'`", 15, kDoneState},
    {"+", 0, kPlusState},
    {"v", 0, 23},
    {"kg", 0, 30},
    {"s", 0, 21},
    {"td", 0, 22},
    {"hbprm", 0, 18},
    {"jlz", 0, 18},
    {"y", 0, kYState},
    {"w", 0, 39},
    {"f", 0, kFState},
    {"c", 0, 20},
    {"n", 0, kNState},
    {"aieuo", 0, kPendingState},
    {"^", 0, kUpState},
    {"aieuo", 2, kDoneState},
    {"-", 0, kDoneState},
    {"'", 3, kDoneState},
    {"aieuo", 2, kDoneState},
    {"y", 0, 37},
    {"aiueo", 2, kDoneState},
    {"h", 3, 23},
    {"y", 4, 37},
    {"aiueo", 1, kDoneState},
    {"szj", 0, 35},
    {"h", 0, 42},
    {"t", 5, 33},
    {"y", 0, kYState},
    {"w", 0, 38},
    {"k", 0, 40},
    {"aieuo", 3, kDoneState},
    {"y", 0, 37},
    {"w", 0, 39},
    {"u", 2, kDoneState},
    {"zs", 1, 35},
    {"iu", 1, kDoneState},
    {"e=", 2, kDoneState},
    {"auo", 1, kDoneState},
    {"a", 1, kDoneState},
    {"aieo", 1, kDoneState},
    {"ea", 1, kDoneState},
    {"^-.+", 1, kDoneState},
    {"aieuo", 1, kDoneState},
    {"", 1, kDoneState},
    {"", 0, kPendingState},
}};

constexpr std::array<std::string_view, 83> kDirectKana{{
    "+a", "a", "+i", "i", "+u", "u", "+e", "e", "+o", "o",
    "ka", "ga", "ki", "gi", "ku", "gu", "ke", "ge", "ko", "go",
    "sa", "za", "si", "zi", "su", "zu", "se", "ze", "so", "zo",
    "ta", "da", "ti", "di", "+tu", "tu", "du", "te", "de", "to",
    "do", "na", "ni", "nu", "ne", "no", "ha", "ba", "pa", "hi",
    "bi", "pi", "hu", "bu", "pu", "he", "be", "pe", "ho", "bo",
    "po", "ma", "mi", "mu", "me", "mo", "+ya", "ya", "+yu", "yu",
    "+yo", "yo", "ra", "ri", "ru", "re", "ro", "+wa", "wa", "wi",
    "we", "wo", "n'",
}};

struct KanaMapping {
  std::string_view input;
  std::array<JisCode, 2> output;
};

constexpr std::array<KanaMapping, 70> kCompoundKana{{
    {"sha", {0x37, 0x63}}, {"shi", {0x37, 0}},
    {"shu", {0x37, 0x65}}, {"she", {0x37, 0x27}},
    {"sho", {0x37, 0x67}}, {"ja", {0x38, 0x63}},
    {"ji", {0x38, 0}}, {"ju", {0x38, 0x65}},
    {"je", {0x38, 0x27}}, {"jo", {0x38, 0x67}},
    {"jya", {0x38, 0x63}}, {"jyu", {0x38, 0x65}},
    {"jyo", {0x38, 0x67}}, {"chi", {0x41, 0}},
    {"ci", {0x41, 0}}, {"cha", {0x41, 0x63}},
    {"chu", {0x41, 0x65}}, {"che", {0x41, 0x27}},
    {"cho", {0x41, 0x67}}, {"tsu", {0x44, 0}},
    {"tzu", {0x44, 0}}, {"dsu", {0x45, 0}},
    {"dzu", {0x45, 0}}, {"+tsu", {0x43, 0}},
    {"+tzu", {0x43, 0}}, {"la", {0x69, 0}},
    {"li", {0x6a, 0}}, {"lu", {0x6b, 0}},
    {"le", {0x6c, 0}}, {"lo", {0x6d, 0}},
    {"lya", {0x6a, 0x63}}, {"lyu", {0x6a, 0x65}},
    {"lyo", {0x6a, 0x67}}, {"fa", {0x55, 0x21}},
    {"fi", {0x55, 0x23}}, {"fu", {0x55, 0}},
    {"fe", {0x55, 0x27}}, {"fo", {0x55, 0x29}},
    {"ye", {0x24, 0x27}}, {"kwa", {0x2f, 0x21}},
    {"kwi", {0x2f, 0x23}}, {"kwe", {0x2f, 0x27}},
    {"kwo", {0x2f, 0x29}}, {"gwa", {0x30, 0x21}},
    {"gwi", {0x30, 0x23}}, {"gwe", {0x30, 0x27}},
    {"gwo", {0x30, 0x29}}, {"n", {0x73, 0}},
    {"tha", {0x46, 0x21}}, {"thi", {0x46, 0x23}},
    {"thu", {0x46, 0x25}}, {"the", {0x46, 0x27}},
    {"tho", {0x46, 0x29}}, {"dha", {0x47, 0x21}},
    {"dhi", {0x47, 0x23}}, {"dhu", {0x47, 0x25}},
    {"dhe", {0x47, 0x27}}, {"dho", {0x47, 0x29}},
    {"ca", {0x2b, 0}}, {"cu", {0x2f, 0}},
    {"ce", {0x3b, 0}}, {"co", {0x33, 0}},
    {"dji", {0x42, 0}}, {"dzi", {0x42, 0}},
    {"tji", {0x42, 0}}, {"tzi", {0x42, 0}},
    {"dsi", {0x24, 0}}, {"tsi", {0x24, 0}},
    {"dju", {0x26, 0}}, {"tju", {0x26, 0}},
}};

constexpr std::array<KanaMapping, 37> kComplexKana{{
    {"va", {0x2574, 0x2521}}, {"vi", {0x2574, 0x2523}},
    {"vu", {0x2574, 0}}, {"ve", {0x2574, 0x2527}},
    {"vo", {0x2574, 0x2529}}, {"+ka", {0x2575, 0}},
    {"+ke", {0x2576, 0}}, {"`", {0x2156, 0}},
    {"'", {0x2157, 0}}, {"y=", {0x216f, 0}},
    {"f-", {0x2172, 0}}, {"^^", {0x2130, 0}},
    {"^.", {0x2126, 0}}, {"^-", {0x2144, 0}},
    {"^+", {0x215c, 0}}, {"^#", {0x2139, 0}},
    {"^*", {0x2228, 0}}, {"^0", {0x217b, 0}},
    {"^,", {0x2124, 0}}, {"^!", {0x2125, 0}},
    {"^:", {0x2145, 0}}, {"^6", {0x222a, 0}},
    {"^4", {0x222b, 0}}, {"^8", {0x222c, 0}},
    {"^2", {0x222d, 0}}, {"^[", {0x215a, 0}},
    {"^]", {0x215b, 0}}, {"^<", {0x2154, 0}},
    {"^>", {0x2155, 0}}, {"^{", {0x2158, 0}},
    {"^}", {0x2159, 0}}, {"^(", {0x214c, 0}},
    {"^)", {0x214d, 0}}, {"^`", {0x2146, 0}},
    {"^'", {0x2147, 0}}, {"^~", {0x2148, 0}},
    {"^\"", {0x2149, 0}},
}};

constexpr std::string_view kReserved = "'`^+lzmjvkgstdnhbprywfcaieuo";
constexpr std::string_view kUpSymbols = "^-.+#(){}[]<>`'~\"!*,:02468";

bool ascii_lower(char value) {
  return value >= 'a' && value <= 'z';
}

bool ascii_upper(char value) {
  return value >= 'A' && value <= 'Z';
}

char lower_ascii(char value) {
  return ascii_upper(value) ? static_cast<char>(value - 'A' + 'a') : value;
}

std::optional<JisCode> direct_offset(std::string_view input) {
  const auto found = std::find(kDirectKana.begin(), kDirectKana.end(), input);
  if (found == kDirectKana.end()) {
    return std::nullopt;
  }
  return static_cast<JisCode>(0x21 +
                              std::distance(kDirectKana.begin(), found));
}

std::optional<JisCode> ascii_to_jascii(char input) {
  if (input >= 'A' && input <= 'Z') {
    return static_cast<JisCode>(0x2341 + input - 'A');
  }
  if (input >= 'a' && input <= 'z') {
    return static_cast<JisCode>(0x2361 + input - 'a');
  }

  constexpr std::array<std::pair<char, JisCode>, 33> kCharacters{{
      {' ', 0x2121}, {',', 0x2122}, {'.', 0x2123}, {':', 0x2127},
      {';', 0x2128}, {'?', 0x2129}, {'!', 0x212a}, {'\"', 0x212b},
      {'^', 0x2130}, {'_', 0x2132}, {'-', 0x213c}, {'/', 0x213f},
      {'\\', 0x2140}, {'~', 0x2141}, {'|', 0x2143}, {'`', 0x2146},
      {'\'', 0x2147}, {'(', 0x214a}, {')', 0x214b}, {'[', 0x214e},
      {']', 0x214f}, {'{', 0x2150}, {'}', 0x2151}, {'<', 0x2152},
      {'>', 0x2153}, {'+', 0x215c}, {'=', 0x2161}, {'$', 0x2170},
      {'%', 0x2173}, {'#', 0x2174}, {'&', 0x2175}, {'*', 0x2176},
      {'@', 0x2177},
  }};
  if (input >= '0' && input <= '9') {
    return static_cast<JisCode>(0x2330 + input - '0');
  }
  const auto found = std::find_if(
      kCharacters.begin(), kCharacters.end(),
      [input](const auto& entry) { return entry.first == input; });
  return found == kCharacters.end()
             ? std::nullopt
             : std::optional<JisCode>(found->second);
}

bool is_hiragana(JisCode code) {
  return (code & 0xff00U) == kHiraganaBase;
}

void append_output(std::vector<KanaInputEvent>& events,
                   const std::array<JisCode, 2>& output, JisCode base,
                   bool& start) {
  for (const JisCode offset : output) {
    if (offset == 0) {
      continue;
    }
    const JisCode code = offset < 0x100 ? static_cast<JisCode>(base + offset)
                                       : offset;
    const KanaInputKind kind =
        is_hiragana(code) ? (start ? KanaInputKind::kKanaStart
                                   : KanaInputKind::kKanaContinue)
                          : KanaInputKind::kText;
    events.push_back({kind, JwpText{code}});
    start = false;
  }
}

}  // namespace

bool KanaInputEvent::operator==(const KanaInputEvent& other) const noexcept {
  return kind == other.kind && text == other.text;
}

KanaInputComposer::KanaInputComposer(KanaInputOptions options)
    : options_(options) {}

std::vector<KanaInputEvent> KanaInputComposer::push_ascii(char input) {
  const unsigned value = static_cast<unsigned char>(input);
  if (value < 0x20U || value > 0x7eU) {
    throw KanaInputError("kana input must be printable 7-bit ASCII");
  }

  std::vector<KanaInputEvent> events;
  char original = input;
  char current = lower_ascii(input);

  if (buffer_.size() == 1 && !pending_output_ && current != 'm' &&
      ((buffer_[0] == original &&
        ((original >= 'A' && original <= 'Z') ||
         (original >= 'a' && original <= 'z'))) ||
       (buffer_[0] == 't' && original == 'c'))) {
    buffer_ = current == original ? "+tu" : "+TU";
    pending_output_ = true;
    flush_pending(events);
  }

  if (buffer_.size() == 1 &&
      (buffer_[0] == 'm' || buffer_[0] == 'M') &&
      (current == '\'' || current == '\"' || current == 'b' ||
       current == 'm' || current == 'p')) {
    buffer_ = buffer_[0] == 'm' ? "n" : "N";
    pending_output_ = true;
    flush_pending(events);
    if (current == '\'' || current == '\"') {
      return events;
    }
  }

  if (!options_.old_katakana_input && pending_output_ &&
      buffer_.size() == 1 && (current == '\'' || current == '\"') &&
      std::string_view("AIUEO").find(buffer_[0]) != std::string_view::npos) {
    flush_pending(events);
    return events;
  }

  if (state_ == kNState && current == '\"') {
    current = '\'';
    original = '\'';
  }

  bool accepted = kReserved.find(current) != std::string_view::npos;
  std::optional<std::size_t> forced_row;
  if (state_ == kUpState &&
      kUpSymbols.find(current) != std::string_view::npos) {
    accepted = true;
    forced_row = kUpState;
  }
  if (state_ == kPlusState && current == '+') {
    accepted = false;
  }
  if ((state_ == kYState && current == '=') ||
      (state_ == kFState && current == '-')) {
    accepted = true;
    if (!buffer_.empty()) {
      buffer_[0] = lower_ascii(buffer_[0]);
    }
  }
  if (!accepted) {
    flush_pending(events);
    if (original == 'x') {
      events.push_back({KanaInputKind::kText, JwpText{0x215f}});
    } else if (const auto code = ascii_to_jascii(original)) {
      events.push_back({KanaInputKind::kText, JwpText{*code}});
    }
    return events;
  }

  while (true) {
    std::optional<std::size_t> row = forced_row;
    forced_row.reset();
    if (!row) {
      if (state_ >= kStates.size()) {
        throw KanaInputError("kana input state is invalid");
      }
      const std::size_t count = kStates[state_].length;
      for (std::size_t index = 0; index < count; ++index) {
        if (kStates[state_ + index].valid.find(current) !=
            std::string_view::npos) {
          row = state_ + index;
          break;
        }
      }
    }

    if (row) {
      if (buffer_.size() >= kMaximumInput) {
        throw KanaInputError("kana input sequence is too long");
      }
      buffer_.push_back(original);
      state_ = kStates[*row].next;
      if (state_ == kPendingState) {
        if (original == current) {
          state_ = kDoneState;
        } else {
          pending_output_ = true;
        }
      }
      if (state_ == kNState) {
        pending_output_ = true;
      }
      if (buffer_.size() > 1) {
        pending_output_ = false;
      }
      if (state_ == kDoneState) {
        if (!emit_buffer(events)) {
          throw KanaInputError("completed kana input could not be converted");
        }
        clear_state();
      }
      return events;
    }

    if (pending_output_ && ascii_upper(buffer_[0]) && ascii_lower(original) &&
        buffer_[0] != 'N') {
      kana_start_ = true;
      buffer_[0] = lower_ascii(buffer_[0]);
    }
    flush_pending(events);
  }
}

std::vector<KanaInputEvent> KanaInputComposer::flush() {
  std::vector<KanaInputEvent> events;
  if (buffer_.empty()) {
    return events;
  }
  if (!emit_buffer(events)) {
    throw KanaInputError("incomplete kana input cannot be flushed");
  }
  clear_state();
  return events;
}

bool KanaInputComposer::discard() noexcept {
  const bool had_input = !buffer_.empty();
  clear_state();
  return had_input;
}

bool KanaInputComposer::pending() const noexcept { return !buffer_.empty(); }

bool KanaInputComposer::pending_ambiguous() const noexcept {
  return pending_output_ && buffer_.size() == 1 &&
         std::string_view("AIUEONn").find(buffer_[0]) !=
             std::string_view::npos;
}

void KanaInputComposer::set_options(KanaInputOptions options) {
  if (options.old_katakana_input == options_.old_katakana_input) {
    return;
  }
  if (pending()) {
    throw KanaInputError("cannot change kana input options while composing");
  }
  options_ = options;
}

bool KanaInputComposer::emit_buffer(
    std::vector<KanaInputEvent>& events) const {
  if (buffer_.empty()) {
    return true;
  }

  std::string normalized = buffer_;
  JisCode base = kKatakanaBase;
  bool start = ascii_upper(normalized.front());
  for (char& value : normalized) {
    if (value == '+' || !ascii_lower(value)) {
      value = lower_ascii(value);
    } else {
      base = kHiraganaBase;
    }
  }
  start = kana_start_ || (start && base == kHiraganaBase);

  if (const auto offset = direct_offset(normalized)) {
    append_output(events, {*offset, 0}, base, start);
    return true;
  }
  for (const KanaMapping& mapping : kCompoundKana) {
    if (mapping.input == normalized) {
      append_output(events, mapping.output, base, start);
      return true;
    }
  }
  for (const KanaMapping& mapping : kComplexKana) {
    if (mapping.input == normalized) {
      append_output(events, mapping.output, 0, start);
      return true;
    }
  }

  if (normalized.size() == 3) {
    std::string first{"xi"};
    first[0] = normalized[0];
    std::string second = normalized;
    second[0] = '+';
    const auto first_offset = direct_offset(first);
    const auto second_offset = direct_offset(second);
    if (first_offset && second_offset) {
      append_output(events, {*first_offset, *second_offset}, base, start);
      return true;
    }
  }
  return false;
}

void KanaInputComposer::clear_state() noexcept {
  buffer_.clear();
  state_ = kBaseState;
  pending_output_ = false;
  kana_start_ = false;
}

void KanaInputComposer::flush_pending(
    std::vector<KanaInputEvent>& events) {
  if (pending_output_ && !emit_buffer(events)) {
    throw KanaInputError("pending kana input could not be converted");
  }
  clear_state();
}

}  // namespace jwpqt::core
