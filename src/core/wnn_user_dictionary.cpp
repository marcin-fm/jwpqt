// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_user_dictionary.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

namespace jwpqt::core {
namespace {

constexpr JisCode kHiraganaBase = 0x2400;
constexpr std::size_t kMaximumEncodedSize = 8U * 1024U * 1024U;

[[noreturn]] void fail(std::size_t entry, std::string_view reason) {
  std::ostringstream message;
  message << "Invalid WNN user entry " << entry << ": " << reason;
  throw WnnUserDictionaryError(message.str());
}

bool is_high_bit_byte(char value) {
  return static_cast<unsigned char>(value) >= 0x80U;
}

JisCode suffix_for_ending(char ending) {
  switch (ending) {
    case '*':
      return 0;
    case '1':
      return 0x246b;
    case 'i':
      return 0x2424;
    case 'u':
      return 0x2426;
    case 'k':
      return 0x242f;
    case 'g':
      return 0x2430;
    case 's':
      return 0x2439;
    case 't':
      return 0x2444;
    case 'n':
      return 0x244c;
    case 'b':
      return 0x2456;
    case 'm':
      return 0x2460;
    case 'r':
      return 0x246b;
    default:
      return 0xffff;
  }
}

std::optional<char> godan_ending_for_suffix(JisCode suffix) {
  switch (suffix) {
    case 0x2426:
      return 'u';
    case 0x242f:
      return 'k';
    case 0x2430:
      return 'g';
    case 0x2439:
      return 's';
    case 0x2444:
      return 't';
    case 0x244c:
      return 'n';
    case 0x2456:
      return 'b';
    case 0x2460:
      return 'm';
    case 0x246b:
      return 'r';
    default:
      return std::nullopt;
  }
}

bool is_ichidan_stem_kana(JisCode kana) {
  switch (kana) {
    case 0x2423:
    case 0x2424:
    case 0x2427:
    case 0x2428:
    case 0x242d:
    case 0x242e:
    case 0x2431:
    case 0x2432:
    case 0x2437:
    case 0x2438:
    case 0x243b:
    case 0x243c:
    case 0x2441:
    case 0x2442:
    case 0x2446:
    case 0x2447:
    case 0x244b:
    case 0x244d:
    case 0x2452:
    case 0x2453:
    case 0x2454:
    case 0x2458:
    case 0x2459:
    case 0x245a:
    case 0x245f:
    case 0x2461:
    case 0x246a:
    case 0x246c:
    case 0x2470:
    case 0x2471:
      return true;
    default:
      return false;
  }
}

void validate_entry(const WnnUserEntry& entry, std::size_t index,
                    std::size_t& candidate_count,
                    std::size_t& candidate_cells) {
  if (entry.reading.empty()) {
    fail(index, "reading is empty");
  }
  const JisCode suffix = suffix_for_ending(entry.ending);
  if (suffix == 0xffff) {
    fail(index, "ending is unsupported");
  }
  const std::size_t suffix_size = suffix == 0 ? 0U : 1U;
  if (entry.reading.size() <= suffix_size ||
      entry.reading.size() - suffix_size > kWnnMaximumKeySize) {
    fail(index, "reading exceeds the legacy limit");
  }
  for (const JisCode code : entry.reading) {
    if ((code & 0xff00U) != kHiraganaBase) {
      fail(index, "reading contains non-hiragana text");
    }
  }

  if (suffix != 0 &&
      entry.reading.back() != suffix) {
    fail(index, "inflected reading has the wrong final kana");
  }
  if (entry.candidates.empty()) {
    fail(index, "candidate list is empty");
  }
  if (entry.candidates.size() >
      kWnnMaximumCandidateCount - candidate_count) {
    fail(index, "dictionary exceeds the candidate limit");
  }
  candidate_count += entry.candidates.size();

  for (const JwpText& candidate : entry.candidates) {
    if (candidate.empty()) {
      fail(index, "candidate is empty");
    }
    if (candidate.size() > kWnnMaximumCandidateCells - candidate_cells) {
      fail(index, "dictionary exceeds the candidate-cell limit");
    }
    candidate_cells += candidate.size();
    for (const JisCode code : candidate) {
      if (code == 0 || (code & 0x8080U) != 0) {
        fail(index, "candidate contains an unrepresentable JWP code");
      }
    }
  }
}

void validate_entries(const std::vector<WnnUserEntry>& entries) {
  if (entries.size() > kWnnMaximumRecordCount) {
    throw WnnUserDictionaryError(
        "WNN user dictionary exceeds the record limit");
  }
  std::size_t candidate_count = 0;
  std::size_t candidate_cells = 0;
  for (std::size_t index = 0; index < entries.size(); ++index) {
    validate_entry(entries[index], index, candidate_count, candidate_cells);
  }
}

std::vector<std::uint8_t> key_for_entry(const WnnUserEntry& entry) {
  const JisCode suffix = suffix_for_ending(entry.ending);
  const std::size_t key_size = entry.reading.size() - (suffix == 0 ? 0U : 1U);
  std::vector<std::uint8_t> key;
  key.reserve(key_size);
  for (std::size_t index = 0; index < key_size; ++index) {
    key.push_back(
        static_cast<std::uint8_t>((entry.reading[index] & 0x007fU) | 0x80U));
  }
  return key;
}

}  // namespace

bool WnnUserEntry::operator==(const WnnUserEntry& other) const noexcept {
  return reading == other.reading && ending == other.ending &&
         candidates == other.candidates;
}

WnnUserEntry make_wnn_user_entry(JwpText reading,
                                 std::vector<JwpText> candidates,
                                 WnnUserInflection inflection) {
  constexpr JisCode kKanaI = 0x2424;
  constexpr JisCode kKanaRu = 0x246b;

  if (reading.empty() || candidates.empty()) {
    throw WnnUserDictionaryError(
        "WNN user conversion reading and candidates must not be empty");
  }

  char ending = '*';
  std::optional<JisCode> removable_suffix;
  if (inflection != WnnUserInflection::kUninflected) {
    if (reading.size() < 2) {
      throw WnnUserDictionaryError(
          "Inflected WNN user conversion needs at least two kana");
    }
    if (candidates.size() != 1) {
      throw WnnUserDictionaryError(
          "Inflected WNN user conversion needs exactly one candidate");
    }
  }

  switch (inflection) {
    case WnnUserInflection::kUninflected:
      break;
    case WnnUserInflection::kGodan: {
      const auto godan = godan_ending_for_suffix(reading.back());
      if (!godan.has_value()) {
        throw WnnUserDictionaryError(
            "Godan WNN user conversion has an invalid final kana");
      }
      ending = *godan;
      removable_suffix = reading.back();
      break;
    }
    case WnnUserInflection::kIchidan:
      if (reading.back() != kKanaRu ||
          !is_ichidan_stem_kana(reading[reading.size() - 2])) {
        throw WnnUserDictionaryError(
            "Ichidan WNN user conversion must be an i/e-stem ending in ru");
      }
      ending = '1';
      removable_suffix = kKanaRu;
      break;
    case WnnUserInflection::kIAdjective:
      if (reading.back() != kKanaI) {
        throw WnnUserDictionaryError(
            "I-adjective WNN user conversion must end in i");
      }
      ending = 'i';
      removable_suffix = kKanaI;
      break;
  }

  if (candidates.front().empty()) {
    throw WnnUserDictionaryError(
        "WNN user conversion candidate must not be empty");
  }
  if (removable_suffix.has_value() &&
      candidates.front().back() == *removable_suffix) {
    candidates.front().pop_back();
    if (candidates.front().empty()) {
      throw WnnUserDictionaryError(
          "WNN user conversion candidate became empty after suffix removal");
    }
  }

  WnnUserEntry entry{std::move(reading), ending, std::move(candidates)};
  WnnUserDictionary::from_entries({entry});
  return entry;
}

WnnUserDictionary WnnUserDictionary::parse(std::string_view bytes) {
  if (bytes.size() > kMaximumEncodedSize) {
    throw WnnUserDictionaryError(
        "WNN user dictionary exceeds the encoded size limit");
  }
  std::vector<WnnUserEntry> entries;
  std::size_t candidate_count = 0;
  std::size_t candidate_cells = 0;
  std::size_t line_start = 0;
  while (line_start < bytes.size()) {
    if (entries.size() == kWnnMaximumRecordCount) {
      throw WnnUserDictionaryError(
          "WNN user dictionary exceeds the record limit");
    }
    const std::size_t line_end = bytes.find('\n', line_start);
    if (line_end == std::string_view::npos) {
      fail(entries.size(), "record is not LF-terminated");
    }

    WnnUserEntry entry;
    std::size_t cursor = line_start;
    while (cursor < line_end && is_high_bit_byte(bytes[cursor])) {
      if (entry.reading.size() == kWnnMaximumKeySize) {
        fail(entries.size(), "reading exceeds the legacy limit");
      }
      const auto value = static_cast<std::uint8_t>(
          static_cast<unsigned char>(bytes[cursor]));
      entry.reading.push_back(
          static_cast<JisCode>(kHiraganaBase | (value & 0x7fU)));
      ++cursor;
    }
    if (entry.reading.empty()) {
      fail(entries.size(), "reading is empty");
    }
    if (cursor >= line_end) {
      fail(entries.size(), "ending and candidate list are missing");
    }
    entry.ending = bytes[cursor++];
    const JisCode suffix = suffix_for_ending(entry.ending);
    if (suffix == 0xffff) {
      fail(entries.size(), "ending is unsupported");
    }
    if (suffix != 0) {
      entry.reading.push_back(suffix);
    }

    if (cursor == line_end) {
      fail(entries.size(), "candidate list is empty");
    }
    if (candidate_count == kWnnMaximumCandidateCount) {
      fail(entries.size(), "dictionary exceeds the candidate limit");
    }
    entry.candidates.emplace_back();
    ++candidate_count;
    while (cursor < line_end) {
      const auto first = static_cast<std::uint8_t>(
          static_cast<unsigned char>(bytes[cursor]));
      if (first == '/') {
        if (entry.candidates.back().empty()) {
          fail(entries.size(), "candidate is empty");
        }
        if (candidate_count == kWnnMaximumCandidateCount) {
          fail(entries.size(), "dictionary exceeds the candidate limit");
        }
        entry.candidates.emplace_back();
        ++candidate_count;
        ++cursor;
        continue;
      }
      if (first < 0x80U || cursor + 1 >= line_end) {
        fail(entries.size(), "candidate contains a partial or ASCII pair");
      }
      const auto second = static_cast<std::uint8_t>(
          static_cast<unsigned char>(bytes[cursor + 1]));
      if (second < 0x80U) {
        fail(entries.size(), "candidate contains an ASCII second byte");
      }
      const JisCode code = static_cast<JisCode>(
          (static_cast<std::uint16_t>(first & 0x7fU) << 8U) |
          static_cast<std::uint16_t>(second & 0x7fU));
      if (code == 0) {
        fail(entries.size(), "candidate contains a zero JWP code");
      }
      if (candidate_cells == kWnnMaximumCandidateCells) {
        fail(entries.size(), "dictionary exceeds the candidate-cell limit");
      }
      entry.candidates.back().push_back(code);
      ++candidate_cells;
      cursor += 2;
    }
    if (entry.candidates.back().empty()) {
      fail(entries.size(), "candidate is empty");
    }
    entries.push_back(std::move(entry));
    line_start = line_end + 1;
  }
  return from_entries(std::move(entries));
}

WnnUserDictionary WnnUserDictionary::from_entries(
    std::vector<WnnUserEntry> entries) {
  validate_entries(entries);
  WnnUserDictionary dictionary;
  dictionary.entries_ = std::move(entries);
  return dictionary;
}

std::string WnnUserDictionary::serialize() const {
  validate_entries(entries_);
  std::string bytes;
  for (const WnnUserEntry& entry : entries_) {
    for (const std::uint8_t key_byte : key_for_entry(entry)) {
      bytes.push_back(static_cast<char>(key_byte));
    }
    bytes.push_back(entry.ending);
    for (std::size_t candidate_index = 0;
         candidate_index < entry.candidates.size(); ++candidate_index) {
      if (candidate_index != 0) {
        bytes.push_back('/');
      }
      for (const JisCode code : entry.candidates[candidate_index]) {
        bytes.push_back(
            static_cast<char>(((code >> 8U) & 0x7fU) | 0x80U));
        bytes.push_back(static_cast<char>((code & 0x7fU) | 0x80U));
      }
    }
    bytes.push_back('\n');
  }
  return bytes;
}

const std::vector<WnnUserEntry>& WnnUserDictionary::entries() const noexcept {
  return entries_;
}

std::vector<WnnRecord> WnnUserDictionary::lookup_records() const {
  std::vector<WnnRecord> records;
  records.reserve(entries_.size());
  for (const WnnUserEntry& entry : entries_) {
    records.push_back(
        {0, key_for_entry(entry), entry.ending, entry.candidates});
  }
  return records;
}

}  // namespace jwpqt::core
