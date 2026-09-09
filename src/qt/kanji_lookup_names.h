// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>

namespace jwpqt::qt {

// Order is the persisted legacy index/reading type, not a filtered combo index.
inline constexpr std::array<const char*, 21> kKanjiIndexNames = {
    "Modern Reader's Japanese-English Character Dictionary, Andrew Nelson",
    "New Nelson Japanese-English Character Dictionary, John Haig",
    "New Japanese-English Character Dictionary, Jack Halpern",
    "School grade", "Morohashi (full index)", "Morohashi (volume/index)",
    "Halpern Kanji Learners' Dictionary", "Spahn-Hadamitzky Kanji & Kana",
    "Henshall", "Gakken", "Heisig", "O'Neill Names", "O'Neill Essential Kanji",
    "De Roo", "Frequency", "Read/Write Japanese", "Tuttle Kanji Cards",
    "The Kanji Way", "Kanji in Context", "Japanese for Busy People", "Compact Kanji Guide"};
inline constexpr std::array<const char*, 7> kKanjiReadingNames = {
    "On-yomi", "Kun-yomi", "On-yomi or kun-yomi", "Meaning", "Nanori", "Pinyin", "Korean"};

}  // namespace jwpqt::qt
