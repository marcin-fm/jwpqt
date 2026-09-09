// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/character_font.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace jwpqt::core {
namespace {
void put16(std::string& out, std::uint32_t value) { out += static_cast<char>(value >> 8); out += static_cast<char>(value); }
void put32(std::string& out, std::uint32_t value) { put16(out, value >> 16); put16(out, value); }
unsigned get16(std::string_view text, std::size_t offset) {
  if (offset > text.size() || text.size() - offset < 2) throw std::invalid_argument("Truncated native font table");
  return (static_cast<unsigned>(static_cast<unsigned char>(text[offset])) << 8) |
      static_cast<unsigned>(static_cast<unsigned char>(text[offset + 1]));
}
void pad(std::string& text) { while (text.size() % 4) text += '\0'; }
std::uint32_t checksum(std::string_view text) {
  std::uint32_t result = 0;
  for (std::size_t i = 0; i < text.size(); i += 4) {
    std::uint32_t word = 0;
    for (unsigned j = 0; j < 4; ++j) word = (word << 8) | (i + j < text.size() ? static_cast<unsigned char>(text[i + j]) : 0);
    result += word;
  }
  return result;
}
}  // namespace

std::optional<std::map<std::uint16_t, std::uint16_t>> vertical_glyph_substitutions(
    std::string_view data, std::uint32_t glyph_count) {
  if (data.empty()) return std::nullopt;
  if (data.size() > 8 * 1024 * 1024 || !glyph_count || glyph_count > 65535 ||
      get16(data, 0) != 1 || get16(data, 2) > 1)
    throw std::invalid_argument("Unsupported vertical font table");
  const auto span = [&](std::size_t at, std::size_t size) {
    if (at > data.size() || size > data.size() - at)
      throw std::invalid_argument("Truncated vertical font table");
  };
  const auto relative = [&](std::size_t base, std::uint32_t offset) {
    if (!offset || base > data.size() || offset > data.size() - base)
      throw std::invalid_argument("Invalid vertical font offset");
    return base + offset;
  };
  span(0, get16(data, 2) == 1 ? 14 : 10);
  const auto features = relative(0, get16(data, 6));
  const auto count = get16(data, features);
  if (count > 4096) throw std::invalid_argument("Vertical feature budget exceeded");
  span(features + 2, count * 6);
  std::size_t feature = 0;
  for (unsigned i = 0; i < count; ++i) {
    const auto record = features + 2 + i * 6;
    if (data.substr(record, 4) == "vert") {
      feature = relative(features, get16(data, record + 4));
      break;
    }
  }
  if (!feature) return std::nullopt;
  const auto feature_count = get16(data, feature + 2);
  if (!feature_count) throw std::invalid_argument("Empty vertical font feature");
  span(feature + 4, feature_count * 2);
  const auto lookups = relative(0, get16(data, 8));
  const auto lookup_count = get16(data, lookups), index = get16(data, feature + 4);
  if (index >= lookup_count) throw std::invalid_argument("Invalid vertical lookup index");
  span(lookups + 2, lookup_count * 2);
  const auto lookup = relative(lookups, get16(data, lookups + 2 + index * 2));
  auto type = get16(data, lookup);
  const auto sub_count = get16(data, lookup + 4);
  if (!sub_count) throw std::invalid_argument("Empty vertical substitution lookup");
  span(lookup + 6, sub_count * 2);
  if (get16(data, lookup + 2) & 0x10) span(lookup + 6 + sub_count * 2, 2);
  auto sub = relative(lookup, get16(data, lookup + 6));
  if (type == 7) {
    if (get16(data, sub) != 1) throw std::invalid_argument("Unsupported vertical extension");
    type = get16(data, sub + 2);
    const auto offset = (static_cast<std::uint32_t>(get16(data, sub + 4)) << 16) | get16(data, sub + 6);
    sub = relative(sub, offset);
  }
  if (type != 1) throw std::invalid_argument("Unsupported vertical substitution lookup");
  const auto format = get16(data, sub);
  if (format != 1 && format != 2) throw std::invalid_argument("Unsupported vertical single substitution");
  const auto value = get16(data, sub + 4);
  if (format == 2) span(sub + 6, value * 2);
  const auto coverage = relative(sub, get16(data, sub + 2));
  const auto coverage_format = get16(data, coverage), coverage_count = get16(data, coverage + 2);
  std::vector<std::uint16_t> from;
  if (coverage_format == 1) {
    span(coverage + 4, coverage_count * 2);
    for (unsigned i = 0; i < coverage_count; ++i) from.push_back(static_cast<std::uint16_t>(get16(data, coverage + 4 + i * 2)));
  } else if (coverage_format == 2) {
    span(coverage + 4, coverage_count * 6);
    for (unsigned i = 0; i < coverage_count; ++i) {
      const auto record = coverage + 4 + i * 6;
      const auto first = get16(data, record), last = get16(data, record + 2);
      if (first > last || get16(data, record + 4) != from.size() ||
          last >= glyph_count || from.size() + last - first + 1 > glyph_count)
        throw std::invalid_argument("Invalid vertical coverage range");
      for (unsigned glyph = first; glyph <= last; ++glyph) from.push_back(static_cast<std::uint16_t>(glyph));
    }
  } else throw std::invalid_argument("Unsupported vertical font coverage");
  if (format == 2 && from.size() != value) throw std::invalid_argument("Vertical substitution count mismatch");
  std::map<std::uint16_t, std::uint16_t> result;
  for (std::size_t i = 0; i < from.size(); ++i) {
    const auto to = format == 1 ? static_cast<std::uint16_t>(from[i] + value) : get16(data, sub + 6 + i * 2);
    if (from[i] >= glyph_count || to >= glyph_count || (i && from[i] <= from[i - 1]))
      throw std::invalid_argument("Invalid vertical glyph mapping");
    result.emplace(from[i], to);
  }
  return result;
}

std::string make_character_font(std::map<std::string, std::string> tables,
                               const std::map<char32_t, std::uint32_t>& mappings,
                               std::string_view family, bool japanese) {
  constexpr std::size_t limit = 64 * 1024 * 1024;
  if (tables.size() > 64 || mappings.empty() || mappings.size() > (japanese ? 8192U : 2048U) || family.empty() || family.size() > 63 ||
      !std::all_of(family.begin(), family.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
      })) throw std::invalid_argument("Invalid private character font request");
  std::size_t total = 0;
  for (const auto& table : tables) {
    if (table.first.size() != 4 || table.second.size() > limit - total)
      throw std::invalid_argument("Native character font table budget exceeded");
    total += table.second.size();
  }
  if (tables["head"].size() < 54 || tables["hhea"].size() < 36 || tables["maxp"].size() < 6 ||
      tables["hmtx"].empty() || (!tables.count("CFF ") && !tables.count("CFF2") && (!tables.count("glyf") || !tables.count("loca"))))
    throw std::invalid_argument("Incomplete native character font");
  const auto glyph_count = get16(tables["maxp"], 4);
  std::string cmap;
  for (unsigned value : {0U, 2U, 0U, 4U}) put16(cmap, value);
  put32(cmap, 20); put16(cmap, 3); put16(cmap, 10); put32(cmap, 20);
  put16(cmap, 12); put16(cmap, 0); put32(cmap, static_cast<std::uint32_t>(16 + 12 * mappings.size()));
  put32(cmap, 0); put32(cmap, static_cast<std::uint32_t>(mappings.size()));
  for (const auto& mapping : mappings) {
    if ((!japanese && mapping.first >= 0x3000) || mapping.first > 0x10ffff ||
        (mapping.first >= 0xd800 && mapping.first <= 0xdfff) ||
        !mapping.second || mapping.second >= glyph_count) throw std::invalid_argument("Invalid native character font mapping");
    put32(cmap, mapping.first); put32(cmap, mapping.first); put32(cmap, mapping.second);
  }
  tables["cmap"] = std::move(cmap);
  // Qt uses OS/2 coverage to choose a face for each script before cmap lookup.
  // A Latin-only cmap must not continue advertising the original CJK coverage.
  if (auto found = tables.find("OS/2"); found != tables.end() && found->second.size() >= 78) {
    auto& os2 = found->second;
    os2.replace(42, 16, 16, '\0');
    // Vertical faces are explicit drawing resources, never script fallbacks
    // for horizontal widgets after application-local registration.
    std::uint32_t ranges = 0;
    if (!japanese) for (const auto& mapping : mappings) {
      const auto c = mapping.first;
      if (c < 128) ranges |= 1U;
      else if (c < 256) ranges |= 2U;
      else if (c < 384) ranges |= 4U;
      else if (c < 592) ranges |= 8U;
      else if (c >= 0x370 && c <= 0x3ff) ranges |= 1U << 7;
      else if (c >= 0x400 && c <= 0x52f) ranges |= 1U << 9;
      else if (c >= 0x590 && c <= 0x5ff) ranges |= 1U << 11;
      else if (c >= 0x600 && c <= 0x6ff) ranges |= 1U << 13;
      else if (c >= 0x2000 && c <= 0x206f) ranges |= 1U << 31;
    }
    std::string value; put32(value, ranges); os2.replace(42, 4, value);
    if (os2.size() >= 86) {
      value.clear(); put32(value, japanese ? 0 : 0x1ff); put32(value, 0); os2.replace(78, 8, value);
    }
  }
  // Keep copyright, licensing, attribution and other original name records.
  const auto original = tables["name"];
  if (get16(original, 0) > 1) throw std::invalid_argument("Unsupported native font name format");
  const auto count = get16(original, 2), storage = get16(original, 4);
  if (count > 4096 || 6 + count * 12 > original.size() || storage < 6 + count * 12)
    throw std::invalid_argument("Invalid native font names");
  std::vector<std::string> records;
  std::string strings;
  for (unsigned i = 0; i < count; ++i) {
    const std::size_t p = 6 + 12 * i;
    const auto id = get16(original, p + 6), length = get16(original, p + 8), offset = get16(original, p + 10);
    if (get16(original, p + 4) >= 0x8000) throw std::invalid_argument("Native language-tagged font names are not supported");
    if (static_cast<std::size_t>(storage) + offset + length > original.size()) throw std::invalid_argument("Truncated native font name");
    if (id == 1 || id == 2 || id == 3 || id == 4 || id == 6 || id == 16 || id == 17 || id == 18 || id == 21 || id == 22) continue;
    if (strings.size() + length > 60000) throw std::invalid_argument("Native font name budget exceeded");
    std::string record = original.substr(p, 10);
    put16(record, static_cast<unsigned>(strings.size()));
    records.push_back(std::move(record));
    strings.append(original, storage + offset, length);
  }
  for (int id : {1, 2, 3, 4, 6}) {
    const auto text = id == 2 ? std::string_view("Regular") : family;
    std::string record;
    for (unsigned value : {3U, 1U, 0x409U, static_cast<unsigned>(id), static_cast<unsigned>(text.size() * 2), static_cast<unsigned>(strings.size())}) put16(record, value);
    records.push_back(std::move(record));
    for (char c : text) put16(strings, static_cast<unsigned char>(c));
  }
  if (6 + records.size() * 12 + strings.size() > 65535) throw std::invalid_argument("Native font names exceed table bounds");
  std::string name; put16(name, 0); put16(name, static_cast<unsigned>(records.size())); put16(name, static_cast<unsigned>(6 + records.size() * 12));
  for (const auto& record : records) name += record;
  tables["name"] = name + strings;
  // JWP draws single-byte characters independently. Keep optional Latin
  // ligatures from introducing unmapped glyphs (and losing PDF text).
  if (auto found = tables.find("GSUB"); found != tables.end()) {
    auto& gsub = found->second;
    const auto features = get16(gsub, 6);
    const auto feature_count = get16(gsub, features);
    if (feature_count > 4096 || features + 2ULL + feature_count * 6ULL > gsub.size())
      throw std::invalid_argument("Invalid native font features");
    for (unsigned i = 0; i < feature_count; ++i) {
      const std::size_t record = features + 2 + 6 * i;
      const auto tag = gsub.substr(record, 4);
      if (tag != "liga" && tag != "clig" && tag != "dlig" && tag != "hlig") continue;
      const std::size_t feature = features + get16(gsub, record + 4);
      (void)get16(gsub, feature + 2);
      gsub.replace(feature + 2, 2, 2, '\0');
    }
  }
  tables.erase("DSIG"); // A signature over the original tables is no longer valid.
  tables["head"].replace(8, 4, 4, '\0');
  std::string result, payload;
  unsigned power = 1, selector = 0;
  while (power * 2 <= tables.size()) { power *= 2; ++selector; }
  put32(result, (tables.count("CFF ") || tables.count("CFF2")) ? 0x4f54544f : 0x10000);
  put16(result, static_cast<unsigned>(tables.size())); put16(result, power * 16);
  put16(result, selector); put16(result, static_cast<unsigned>(tables.size() * 16 - power * 16));
  std::size_t head_offset = 0;
  for (const auto& table : tables) {
    const auto offset = 12 + tables.size() * 16 + payload.size();
    if (table.second.size() > limit || offset + table.second.size() + 3 > limit) throw std::invalid_argument("Private font size exceeded");
    if (table.first == "head") head_offset = offset;
    result += table.first; put32(result, checksum(table.second)); put32(result, static_cast<std::uint32_t>(offset)); put32(result, static_cast<std::uint32_t>(table.second.size()));
    payload += table.second; pad(payload);
  }
  result += payload;
  std::string adjustment; put32(adjustment, 0xb1b0afbaU - checksum(result));
  result.replace(head_offset + 8, 4, adjustment);
  return result;
}
}  // namespace jwpqt::core
