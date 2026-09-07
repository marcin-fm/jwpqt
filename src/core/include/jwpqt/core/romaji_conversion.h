// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_ROMAJI_CONVERSION_H
#define JWPQT_CORE_ROMAJI_CONVERSION_H

#include <cstddef>
#include <string_view>

#include "jwpqt/core/kana_input.h"

namespace jwpqt::core {

class WnnConversionSession;

// Replays one selected ASCII/tab span. Capitalized automatic spans may use WNN;
// preparation never changes its session or preferences. Invalid replay throws
// before returning any output, unlike the legacy per-character document edits.
// maximum_cells can tighten, but not exceed, the 65535-cell selection limit.
JwpText convert_romaji_text(std::string_view input,
                            const WnnConversionSession* session = nullptr,
                            KanaInputOptions options = {},
                            std::size_t maximum_cells = 65535);

}  // namespace jwpqt::core

#endif
