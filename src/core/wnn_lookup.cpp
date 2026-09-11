// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/wnn_lookup.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "jwpqt/core/kana_input.h"

namespace jwpqt::core {
namespace {

constexpr JisCode kHiraganaBase = 0x2400;
constexpr JisCode kKatakanaBase = 0x2500;
constexpr JisCode kFirstUnsupportedKatakana = 0x2574;
constexpr JisCode kSmallTsuCell = 0x43;
constexpr JisCode kWaCell = 0x6f;
constexpr JisCode kNCell = 0x73;

enum class LookupPass {
  kExact,
  kSpecialStem,
  kConjugated,
};

struct JwpTextHash {
  std::size_t operator()(const JwpText& text) const noexcept {
    std::uint64_t hash = UINT64_C(0xcbf29ce484222325);
    for (const JisCode code : text) {
      hash ^= static_cast<std::uint64_t>(code);
      hash *= UINT64_C(0x100000001b3);
    }
    if constexpr (sizeof(std::size_t) < sizeof(hash)) {
      hash ^= hash >> 32U;
    }
    return static_cast<std::size_t>(hash);
  }
};

class CandidateCollector {
 public:
  explicit CandidateCollector(std::size_t maximum_cells)
      : maximum_cells_(maximum_cells) {}

  void add(const JwpText& source, std::optional<JisCode> suffix,
           bool original_kana) {
    if (examined_ == kWnnMaximumCandidateCount + 1U) {
      throw WnnLookupError("WNN lookup exceeds the candidate limit");
    }
    ++examined_;
    if (source.size() > maximum_cells_ ||
        (suffix && source.size() == maximum_cells_)) {
      throw WnnLookupError("WNN candidate list exceeds the output limit");
    }

    JwpText candidate = source;
    if (suffix) {
      candidate.push_back(*suffix);
    }
    if (!seen_.insert(candidate).second) {
      return;
    }

    const std::size_t separator = candidates_.empty() ? 0U : 1U;
    if (separator > maximum_cells_ - cells_ ||
        candidate.size() > maximum_cells_ - cells_ - separator) {
      throw WnnLookupError("WNN candidate list exceeds the output limit");
    }
    cells_ += separator;
    const std::size_t offset = cells_;
    cells_ += candidate.size();
    candidates_.push_back({std::move(candidate), offset, original_kana});
  }

  bool empty() const noexcept { return candidates_.empty(); }

  std::vector<WnnCandidate> take() { return std::move(candidates_); }

 private:
  std::size_t maximum_cells_;
  std::size_t cells_ = 0;
  std::size_t examined_ = 0;
  std::unordered_set<JwpText, JwpTextHash> seen_;
  std::vector<WnnCandidate> candidates_;
};

bool valid_input_kana(JisCode kana) {
  const JisCode page = static_cast<JisCode>(kana & 0xff00U);
  return page == kHiraganaBase ||
         (page == kKatakanaBase && kana < kFirstUnsupportedKatakana);
}

std::vector<std::uint8_t> make_key(const JwpText& input) {
  if (input.size() > kWnnMaximumKeySize) {
    throw WnnLookupError("WNN conversion key exceeds the legacy limit");
  }
  std::vector<std::uint8_t> key;
  key.reserve(input.size());
  for (const JisCode kana : input) {
    if (!valid_input_kana(kana)) {
      throw WnnLookupError("WNN conversion input contains non-kana text");
    }
    key.push_back(static_cast<std::uint8_t>((kana & 0x00ffU) | 0x80U));
  }
  return key;
}

bool starts_with(const std::vector<std::uint8_t>& value,
                 const std::vector<std::uint8_t>& prefix) {
  return value.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), value.begin());
}

std::optional<char> ending_for_kana(JisCode kana) {
  const auto romaji = romaji_for_kana(kana);
  if (!romaji || romaji->empty()) {
    return std::nullopt;
  }
  const std::size_t offset = romaji->front() == '+' ? 1U : 0U;
  if (offset == romaji->size()) {
    return std::nullopt;
  }
  return (*romaji)[offset];
}

bool ending_matches(char dictionary_ending, char requested_ending,
                    JisCode kana) {
  if (dictionary_ending == '1' || dictionary_ending == 'i' ||
      dictionary_ending == requested_ending) {
    return true;
  }
  const JisCode cell = static_cast<JisCode>(kana & 0x007fU);
  switch (dictionary_ending) {
    case 'u':
      if (requested_ending == 'e' || requested_ending == 'o' ||
          cell == kWaCell || cell == kSmallTsuCell) {
        return true;
      }
      [[fallthrough]];
    case 'k':
    case 'g':
      return requested_ending == 'i';
    case 'r':
    case 'd':
      return cell == kSmallTsuCell;
    case 'f':
    case 'b':
    case 'p':
    case 'm':
      return cell == kNCell;
    default:
      return false;
  }
}

bool record_matches(const WnnRecord& record,
                    const std::vector<std::uint8_t>& key, LookupPass pass,
                    JisCode suffix, std::optional<char> suffix_ending) {
  if (record.key != key) {
    return false;
  }
  switch (pass) {
    case LookupPass::kExact:
      return record.ending == '*';
    case LookupPass::kSpecialStem:
      return record.ending == '1' || record.ending == 'i';
    case LookupPass::kConjugated:
      return suffix_ending &&
             ending_matches(record.ending, *suffix_ending, suffix);
  }
  return false;
}

void append_matches(const std::vector<WnnRecord>& records,
                    const std::vector<std::uint8_t>& key, LookupPass pass,
                    JisCode suffix, std::optional<char> suffix_ending,
                    CandidateCollector& output) {
  for (const WnnRecord& record : records) {
    if (!record_matches(record, key, pass, suffix, suffix_ending)) {
      continue;
    }
    for (const JwpText& candidate : record.candidates) {
      output.add(candidate,
                 pass == LookupPass::kConjugated
                     ? std::optional<JisCode>(suffix)
                     : std::nullopt,
                 false);
    }
    if (pass == LookupPass::kSpecialStem) {
      break;
    }
  }
}

void append_pass(const WnnDictionary& system_dictionary,
                 const std::vector<WnnRecord>* user_records,
                 const std::vector<std::uint8_t>& key, LookupPass pass,
                 JisCode suffix, std::optional<char> suffix_ending,
                 CandidateCollector& output) {
  append_matches(system_dictionary.records(), key, pass, suffix,
                 suffix_ending, output);
  if (user_records != nullptr) {
    append_matches(*user_records, key, pass, suffix, suffix_ending, output);
  }
}

bool has_extension(const std::vector<WnnRecord>& records,
                   const std::vector<std::uint8_t>& key) {
  return std::any_of(records.begin(), records.end(), [&key](const auto& record) {
    return starts_with(record.key, key) &&
           (record.key.size() > key.size() || record.ending != '*');
  });
}

}  // namespace

bool WnnCandidate::operator==(const WnnCandidate& other) const noexcept {
  return text == other.text && legacy_cell_offset == other.legacy_cell_offset &&
         original_kana == other.original_kana;
}

WnnLookupResult lookup_wnn_candidates(
    const WnnDictionary& system_dictionary, const JwpText& input,
    const std::vector<WnnRecord>* user_records,
    std::size_t maximum_output_cells) {
  const std::vector<std::uint8_t> full_key = make_key(input);
  WnnLookupResult result;
  if (user_records != nullptr &&
      user_records->size() > kWnnMaximumRecordCount) {
    throw WnnLookupError("WNN user dictionary exceeds the record limit");
  }
  result.can_extend = has_extension(system_dictionary.records(), full_key) ||
                      (user_records != nullptr &&
                       has_extension(*user_records, full_key));
  if (input.empty()) {
    return result;
  }

  CandidateCollector candidates(maximum_output_cells);
  append_pass(system_dictionary, user_records, full_key, LookupPass::kExact, 0,
              std::nullopt, candidates);
  if (input.size() > 1) {
    append_pass(system_dictionary, user_records, full_key,
                LookupPass::kSpecialStem, 0, std::nullopt, candidates);

    std::vector<std::uint8_t> stem_key = full_key;
    stem_key.pop_back();
    const JisCode suffix = input.back();
    append_pass(system_dictionary, user_records, stem_key,
                LookupPass::kConjugated, suffix, ending_for_kana(suffix),
                candidates);
  }

  if (candidates.empty()) {
    return result;
  }
  candidates.add(input, std::nullopt, true);
  result.candidates = candidates.take();
  return result;
}

}  // namespace jwpqt::core
