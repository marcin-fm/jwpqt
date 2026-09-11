// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include "jwpqt/core/jwp_configuration.h"

namespace jwpqt::qt {
// Wire IDs from jwp_stat.h:74-113. Zero is a separator.
inline constexpr std::array<const char*, 37> kToolbarCommands{{
    "", "newDocumentAction", "openDocumentAction", "saveDocumentAction", "deleteDocumentAction",
    "printAction", "undoAction", "redoAction", "kanaInputAction", "asciiInputAction", "jasciiInputAction",
    "cutAction", "copyAction", "pasteAction", "findAction", "replaceAction", "findNextAction",
    "findPreviousAction", "convertSelectionAction", "kanjiInfoAction", "radicalLookupAction",
    "bushuLookupAction", "strokeBushuLookupAction", "skipLookupAction", "spahnLookupAction",
    "fourCornerLookupAction", "kanjiReadingLookupAction", "indexLookupAction", "jisTableAction",
    "kanjiCountAction", "makeKanjiColorListAction", "formatFileAction", "formatParagraphAction",
    "pageLayoutAction", "edictLookupAction", "userDictionaryAction", "applicationOptionsAction"}};
struct ToolbarSettings {
  std::array<std::uint8_t, 100> buttons{{0,1,2,3,0,5,0,11,12,13,0,6,7,0,14,15,16,0,
      8,9,10,18,0,19,28,34,29,0,20,21,22,23,24,25,26,27,0,33,36}};
  int count = 39;
  int area = 0; // top, bottom, left, right
  int icon_size = 16;
  int text_style = 0; // icons, beside, below, text only
  bool locked = false;
};
inline void validate_toolbar(const ToolbarSettings& settings) {
  if (settings.count < 0 || settings.count > 100 || settings.area < 0 || settings.area > 3 ||
      settings.icon_size < 16 || settings.icon_size > 48 || settings.text_style < 0 || settings.text_style > 3)
    throw core::JwpConfigurationError("Invalid toolbar settings");
  for (int i = 0; i < settings.count; ++i)
    if (settings.buttons[i] >= kToolbarCommands.size()) throw core::JwpConfigurationError("Unknown toolbar command ID");
}
}
