// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::core {

enum class EdictRegistryWireEncoding {
  kAnsiBytes,
  kUtf16Le,
};

enum class EdictRegistryEncoding {
  kEucJp,
  kUtf8,
  kMixed,
};

enum class EdictRegistryNames {
  kNone,
  kNames,
  kNamesOnly,
};

enum class EdictRegistrySpecial {
  kNormal,
  kClassical,
  kUser,
};

struct EdictRegistryEntry {
  // ANSI-wire registries store each opaque source byte in the same-numbered
  // code unit. UTF-16LE registries store Unicode. Decode ANSI for display in
  // the migration layer where the historical Windows code page is known.
  std::u16string label;
  std::u16string path;
  EdictRegistryEncoding encoding = EdictRegistryEncoding::kEucJp;
  EdictRegistryNames names = EdictRegistryNames::kNone;
  EdictRegistrySpecial special = EdictRegistrySpecial::kNormal;
  bool indexed = false;
  bool buffered = false;
  bool searched = false;
  bool keep = false;
  bool quiet = false;

  bool operator==(const EdictRegistryEntry& other) const noexcept;
};

struct EdictRegistry {
  EdictRegistryWireEncoding wire_encoding =
      EdictRegistryWireEncoding::kUtf16Le;
  std::vector<EdictRegistryEntry> entries;

  bool operator==(const EdictRegistry& other) const noexcept;
};

struct EdictRegistryLimits {
  std::size_t encoded_bytes = 1024U * 1024U;
  std::size_t entries = 4096;
  std::size_t field_code_units = 64U * 1024U;
};

struct EdictDictionarySample {
  EdictRegistryEncoding encoding = EdictRegistryEncoding::kEucJp;
  std::optional<std::u32string> description;
};

class EdictRegistryError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

EdictRegistry parse_edict_registry(
    std::string_view bytes,
    const EdictRegistryLimits& limits = EdictRegistryLimits{});
std::string serialize_edict_registry(
    const EdictRegistry& registry,
    const EdictRegistryLimits& limits = EdictRegistryLimits{});
EdictDictionarySample infer_edict_dictionary_sample(
    std::string_view bytes,
    LegacyCodePage mixed_code_page = kDefaultLegacyCodePage,
    std::size_t inspected_bytes = 2048);

}  // namespace jwpqt::core
