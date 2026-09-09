// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace jwpqt::core {

// Preserve outlines, metrics and license metadata while restricting Unicode
// coverage to ASCII/legacy extensions below U+3000. Tables must come from a
// validated native OpenType font; Japanese coverage is deliberately forbidden.
std::string make_character_font(std::map<std::string, std::string> tables,
                               const std::map<char32_t, std::uint32_t>& mappings,
                               std::string_view family);

}  // namespace jwpqt::core
