// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/raster_font.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "jwpqt/core/jis_unicode.h"

namespace jwpqt::core {
namespace {
constexpr std::size_t kMaximumFace = 64 * 1024 * 1024;
unsigned le16(std::string_view s, std::size_t p) {
  return static_cast<unsigned char>(s[p]) | (static_cast<unsigned>(static_cast<unsigned char>(s[p + 1])) << 8);
}
void u16(std::string& s, int value) {
  const auto v = static_cast<std::uint16_t>(value);
  s.push_back(static_cast<char>(v >> 8)); s.push_back(static_cast<char>(v));
}
void u32(std::string& s, std::uint32_t v) {
  u16(s, static_cast<int>(v >> 16)); u16(s, static_cast<int>(v & 65535));
}
void pad(std::string& s) { while (s.size() % 4) s.push_back(0); }
std::uint32_t checksum(std::string_view s) {
  std::uint32_t sum = 0;
  for (std::size_t p = 0; p < s.size(); p += 4) {
    std::uint32_t word = 0;
    for (std::size_t i = 0; i < 4; ++i)
      word = (word << 8) | (p + i < s.size() ? static_cast<unsigned char>(s[p + i]) : 0U);
    sum += word;
  }
  return sum;
}
}  // namespace

RasterFont::RasterFont(std::string_view bytes) {
  if (bytes.size() < 64 || bytes.size() > 8 * 1024 * 1024) throw RasterFontError("Invalid raster font size");
  width_ = static_cast<int>(le16(bytes, 40)); height_ = static_cast<int>(le16(bytes, 42));
  glyph_bytes_ = le16(bytes, 44);
  const auto verticals = le16(bytes, 46);
  const auto offset = le16(bytes, 48) | (le16(bytes, 50) << 16);
  const auto holes = le16(bytes, 52);
  leading_ = static_cast<int>(le16(bytes, 54)); spacing_ = static_cast<int>(le16(bytes, 56));
  if (width_ < 8 || width_ > 64 || width_ % 8 || height_ < 1 || height_ > 64 ||
      offset != 64 || holes > 1 || verticals > 256 || leading_ > 64 || spacing_ > 64 ||
      !glyph_bytes_ || glyph_bytes_ % static_cast<std::size_t>(height_))
    throw RasterFontError("Invalid raster font header");
  stride_ = glyph_bytes_ / static_cast<std::size_t>(height_);
  if (stride_ != static_cast<std::size_t>(width_ / 8) &&
      stride_ != static_cast<std::size_t>(((width_ + 15) / 16) * 2))
    throw RasterFontError("Invalid raster font row alignment");
  if ((bytes.size() - 64) % glyph_bytes_) throw RasterFontError("Truncated raster font glyph");
  count_ = (bytes.size() - 64) / glyph_bytes_;
  if (count_ < verticals) throw RasterFontError("Invalid raster font vertical count");
  count_ -= verticals;
  holes_ = holes != 0;
  if (holes_ ? (count_ != 7802 && count_ != 7806) : (count_ != 6874 && count_ != 6878))
    throw RasterFontError("Incomplete raster font character table");
  data_.assign(bytes.substr(64, count_ * glyph_bytes_));
}

std::size_t RasterFont::glyph_index(JisCode code) const noexcept {
  const unsigned hi = code >> 8, lo = code & 255;
  constexpr std::size_t bad = 96;  // 0x2223, valid in both source layouts.
  if (lo < 0x21 || lo > 0x7e || hi < 0x21 || hi > 0x74 || (hi == 0x74 && lo > 0x24)) return bad;
  const auto grid = static_cast<std::size_t>(94 * (hi - 33) + lo - 33);
  if (holes_) return grid < count_ ? grid : bad;
  constexpr std::array<std::array<std::size_t, 3>, 12> ranges{{
      {0,107,0}, {203,212,56}, {220,245,63}, {252,277,69}, {282,364,73},
      {376,461,84}, {470,493,92}, {502,525,100}, {564,596,138},
      {612,644,153}, {1410,4374,886}, {4418,7805,928}}};
  for (const auto& r : ranges)
    if (grid >= r[0] && grid <= r[1]) return grid - r[2] < count_ ? grid - r[2] : bad;
  return bad;
}

bool RasterFont::ink(std::size_t glyph, int x, int y) const {
  if (glyph >= count_ || x < 0 || x >= width_ || y < 0 || y >= height_)
    throw RasterFontError("Raster font pixel outside glyph");
  const auto p = glyph * glyph_bytes_ + static_cast<std::size_t>(y) * stride_ + static_cast<std::size_t>(x / 8);
  return (static_cast<unsigned char>(data_[p]) & (0x80U >> (x % 8))) == 0;
}

std::string RasterFont::native_face(std::string_view family) const {
  if (family.empty() || family.size() > 63 || !std::all_of(family.begin(), family.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
      })) throw RasterFontError("Invalid private raster font family");
  std::map<std::string, std::string> tables;
  auto& glyphs = tables["glyf"]; auto& locations = tables["loca"]; auto& metrics = tables["hmtx"];
  int max_points = 0, max_contours = 0;
  const int advance = (width_ + spacing_ / 2) * 64, shift = (spacing_ / 4) * 64;
  const int em = height_ * 64;
  for (std::size_t g = 0; g <= count_; ++g) {
    u32(locations, static_cast<std::uint32_t>(glyphs.size()));
    u16(metrics, advance); u16(metrics, shift);
    std::vector<std::array<int, 4>> rectangles;
    const auto source = g ? g - 1 : glyph_index(0x2223);
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_;) {
        if (!ink(source, x, y)) { ++x; continue; }
        const int begin = x++;
        while (x < width_ && ink(source, x, y)) ++x;
        rectangles.push_back({begin * 64 + shift, (height_ - y - 1) * 64, x * 64 + shift, (height_ - y) * 64});
      }
    }
    if (rectangles.empty()) continue;
    const int contours = static_cast<int>(rectangles.size()), points = contours * 4;
    max_points = std::max(max_points, points); max_contours = std::max(max_contours, contours);
    u16(glyphs, contours); u16(glyphs, shift); u16(glyphs, 0);
    u16(glyphs, width_ * 64 + shift); u16(glyphs, em);
    for (int i = 0; i < contours; ++i) u16(glyphs, i * 4 + 3);
    u16(glyphs, 0);  // No hinting program; source pixels are the geometry.
    glyphs.append(static_cast<std::size_t>(points), '\1');
    int previous = 0;
    for (const auto& r : rectangles) for (int x : {r[0],r[0],r[2],r[2]}) { u16(glyphs, x - previous); previous = x; }
    previous = 0;
    for (const auto& r : rectangles) for (int y : {r[1],r[3],r[3],r[1]}) { u16(glyphs, y - previous); previous = y; }
    pad(glyphs);
    if (glyphs.size() > kMaximumFace - 1024 * 1024) throw RasterFontError("Raster outline budget exceeded");
  }
  u32(locations, static_cast<std::uint32_t>(glyphs.size()));
  const int number = static_cast<int>(count_ + 1);
  auto& head = tables["head"];
  u32(head, 0x10000); u32(head, 0x10000); u32(head, 0); u32(head, 0x5f0f3cf5);
  u16(head, 3); u16(head, em); head.append(16, '\0');
  for (int v : {0,0,advance,em,0,height_,2,1,0}) u16(head, v);
  auto& hhea = tables["hhea"];
  u32(hhea, 0x10000);
  for (int v : {em,0,leading_ * 64,advance,0,0,advance,1,0,0,0,0,0,0,0,number}) u16(hhea, v);
  auto& maxp = tables["maxp"];
  u32(maxp, 0x10000);
  for (int v : {number,max_points,max_contours,0,0,1,0,0,0,0,0,0,0,0}) u16(maxp, v);
  auto& os2 = tables["OS/2"];
  for (int v : {0,advance,400,5,2}) u16(os2, v);  // Restricted font embedding.
  os2.append(52, '\0'); u16(os2, 0x40); u16(os2, 0x21); u16(os2, 0xffef);
  // Script metadata participates in Qt fallback ordering as well as cmap.
  // Advertise the Japanese coverage actually supplied by the JIS mapping.
  std::string ranges;
  u32(ranges, (1U << 7) | (1U << 9) | (1U << 31));
  u32(ranges, (1U << 16) | (1U << 17) | (1U << 18) | (1U << 27));
  u32(ranges, 1U << 4); u32(ranges, 0);
  os2.replace(42, 16, ranges);
  for (int v : {em,0,leading_ * 64,em,0}) u16(os2, v);
  auto& post = tables["post"]; u32(post, 0x30000); post.append(28, '\0');
  std::map<char32_t, std::uint32_t> mappings;
  for (unsigned hi = 0x21; hi <= 0x74; ++hi) for (unsigned lo = 0x21; lo <= 0x7e; ++lo) {
    const auto code = static_cast<JisCode>((hi << 8) | lo);
    if (const auto scalar = jis_x0208_to_unicode(code)) mappings.emplace(*scalar, static_cast<std::uint32_t>(glyph_index(code) + 1));
  }
  auto& cmap = tables["cmap"];
  for (int v : {0,2,0,4}) u16(cmap, v);
  u32(cmap, 20); u16(cmap, 3); u16(cmap, 10); u32(cmap, 20);
  u16(cmap, 12); u16(cmap, 0); u32(cmap, static_cast<std::uint32_t>(16 + 12 * mappings.size()));
  u32(cmap, 0); u32(cmap, static_cast<std::uint32_t>(mappings.size()));
  for (const auto& pair : mappings) { u32(cmap, pair.first); u32(cmap, pair.first); u32(cmap, pair.second); }
  auto& name = tables["name"];
  std::string strings;
  u16(name, 0); u16(name, 6); u16(name, 78);
  for (int id : {1,2,3,4,5,6}) {
    const auto text = id == 2 ? std::string_view("Regular") : id == 5 ? std::string_view("Version 1.0") : family;
    for (int v : {3,1,0x409,id,static_cast<int>(text.size() * 2),static_cast<int>(strings.size())}) u16(name, v);
    for (char c : text) u16(strings, c);
  }
  name += strings;
  std::string result;
  const int table_count = static_cast<int>(tables.size());
  int power = 1, selector = 0;
  while (power * 2 <= table_count) { power *= 2; ++selector; }
  u32(result, 0x10000); u16(result, table_count); u16(result, power * 16);
  u16(result, selector); u16(result, table_count * 16 - power * 16);
  std::string payload;
  std::size_t head_offset = 0;
  for (auto& table : tables) {
    const auto offset = static_cast<std::uint32_t>(12 + tables.size() * 16 + payload.size());
    if (table.first == "head") head_offset = offset;
    result += table.first; u32(result, checksum(table.second)); u32(result, offset);
    u32(result, static_cast<std::uint32_t>(table.second.size()));
    payload += table.second; pad(payload);
  }
  result += payload;
  if (result.size() > kMaximumFace) throw RasterFontError("Native raster face size exceeded");
  std::string adjustment; u32(adjustment, 0xb1b0afbaU - checksum(result));
  result.replace(head_offset + 8, 4, adjustment);
  return result;
}

}  // namespace jwpqt::core
