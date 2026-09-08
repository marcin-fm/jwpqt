// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/unicode_search.h"

namespace jwpqt::core {
std::vector<std::pair<std::size_t, std::size_t>> find_unicode_text(
    std::u32string_view source, std::u32string_view pattern,
    JwpSearchOptions options, std::size_t maximum_matches, std::size_t* remaining_work, bool overlapping,
    bool first_only) {
  if (pattern.empty()) throw JwpSearchError("Enter text to find");
  if (source.size() > 33554432 || pattern.size() > 65535)
    throw JwpSearchError("Search text exceeds its size limit");
  auto valid = [](std::u32string_view text) {
    for (char32_t ch : text)
      if (ch > 0x10ffff || (ch >= 0xd800 && ch <= 0xdfff))
        throw JwpSearchError("Invalid Unicode search text");
  };
  valid(source);
  valid(pattern);
  std::size_t local_work = 100000000;
  auto& work = remaining_work ? *remaining_work : local_work;
  auto fold = [&](char32_t ch) {
    if (options.jascii_ascii_equivalence &&
        ((ch >= 0xff10 && ch <= 0xff19) || (ch >= 0xff21 && ch <= 0xff3a) ||
         (ch >= 0xff41 && ch <= 0xff5a))) ch -= 0xfee0;
    if (options.ignore_ascii_case && ch >= U'A' && ch <= U'Z') ch += U'a' - U'A';
    return ch;
  };
  std::vector<std::pair<std::size_t, std::size_t>> matches;
  for (std::size_t at = 0; at <= source.size() && pattern.size() <= source.size() - at;) {
    std::size_t n = 0;
    for (; n < pattern.size(); ++n) {
      if (!work) throw JwpSearchError("Search work limit exceeded");
      --work;
      if (fold(source[at + n]) != fold(pattern[n])) break;
    }
    if (n == pattern.size()) {
      if (matches.size() == maximum_matches) throw JwpSearchError("Too many search matches");
      matches.emplace_back(at, at + n);
      if (first_only) return matches;
      at += overlapping ? 1 : n;
    } else ++at;
  }
  return matches;
}
}
