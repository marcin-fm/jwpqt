// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace jwpqt::core {

// Preserve outlines, metrics and license metadata while restricting Unicode
// coverage to ASCII/legacy extensions below U+3000, or an explicitly requested
// Japanese face. Tables must come from a validated native OpenType font.
std::string make_character_font(std::map<std::string, std::string> tables,
                               const std::map<char32_t, std::uint32_t>& mappings,
                               std::string_view family, bool japanese = false);

// Source-style first vert feature/lookup/subtable. Missing features return null;
// malformed or unsupported selected substitutions throw, never read unchecked.
std::optional<std::map<std::uint16_t, std::uint16_t>> vertical_glyph_substitutions(
    std::string_view gsub, std::uint32_t glyph_count);

}  // namespace jwpqt::core
