// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_registry.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/text_file.h"
#include "jwpqt/core/utf8.h"

namespace jwpqt::core {
namespace {

constexpr std::uint32_t kAnsiMagic = 0x12bc3e76U;
constexpr std::uint32_t kUtf16Magic = 0x52bc3eb7U;

unsigned char sample_byte(std::string_view bytes, std::size_t index) {
  return index < bytes.size()
             ? static_cast<unsigned char>(bytes[index])
             : 0;
}

bool source_utf8_sample(std::string_view bytes, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    const unsigned char byte = sample_byte(bytes, i);
    if ((byte & 0x80U) == 0) continue;
    std::size_t continuation = 0;
    if ((byte & 0xe0U) == 0xc0U) continuation = 1;
    else if ((byte & 0xf0U) == 0xe0U) continuation = 2;
    else if ((byte & 0xf8U) == 0xf0U) continuation = 3;
    else return false;
    for (std::size_t j = 1; j <= continuation; ++j)
      if ((sample_byte(bytes, i + j) & 0xc0U) != 0x80U) return false;
    i += continuation;
  }
  return true;
}

bool source_euc_sample(std::string_view bytes, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    if (sample_byte(bytes, i) <= 0x7fU) continue;
    if (sample_byte(bytes, i + 1) < 0x80U) return false;
    ++i;
  }
  return true;
}

std::optional<std::u32string> sample_description(
    std::string_view bytes, EdictRegistryEncoding encoding,
    LegacyCodePage mixed_code_page, std::size_t size) {
  const std::size_t nul = bytes.find('\0');
  const std::size_t available = std::min({size, bytes.size(), nul});
  const std::string_view sample = bytes.substr(0, available);
  const std::size_t first = sample.find('/');
  if (first == std::string_view::npos) return std::nullopt;
  const std::size_t second = sample.find('/', first + 1);
  if (second == std::string_view::npos || second == first + 1)
    return std::nullopt;
  const std::string_view description =
      sample.substr(first + 1, second - first - 1);
  try {
    if (encoding == EdictRegistryEncoding::kUtf8)
      return decode_utf8(description);
    if (encoding == EdictRegistryEncoding::kEucJp)
      return decode_text_file(description, TextEncoding::kEucJp).text;
    std::u32string result;
    result.reserve(description.size());
    for (const char source_byte : description) {
      const auto byte = static_cast<unsigned char>(source_byte);
      const auto code_point = legacy_byte_to_unicode(byte, mixed_code_page);
      if (!code_point) return std::nullopt;
      result.push_back(*code_point);
    }
    return result;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

void validate_limits(const EdictRegistryLimits& limits) {
  if (limits.encoded_bytes < 8 || limits.entries == 0 ||
      limits.field_code_units == 0) {
    throw EdictRegistryError("EDICT registry limits are invalid");
  }
}

void validate_utf16(std::u16string_view value, std::string_view field) {
  for (std::size_t i = 0; i < value.size(); ++i) {
    const std::uint16_t unit = static_cast<std::uint16_t>(value[i]);
    if (unit == 0) {
      throw EdictRegistryError(std::string(field) + " contains a NUL");
    }
    if (unit >= 0xd800U && unit <= 0xdbffU) {
      if (++i >= value.size()) {
        throw EdictRegistryError(std::string(field) +
                                 " has an unpaired high surrogate");
      }
      const std::uint16_t low = static_cast<std::uint16_t>(value[i]);
      if (low < 0xdc00U || low > 0xdfffU) {
        throw EdictRegistryError(std::string(field) +
                                 " has an unpaired high surrogate");
      }
    } else if (unit >= 0xdc00U && unit <= 0xdfffU) {
      throw EdictRegistryError(std::string(field) +
                               " has an unpaired low surrogate");
    }
  }
}

bool at_terminator(std::string_view bytes, std::size_t position) {
  return bytes.size() - position >= 4 && bytes[position] == '\0' &&
         bytes[position + 1] == '\0' && bytes[position + 2] == '\0' &&
         bytes[position + 3] == '\0';
}

std::u16string read_string(ByteReader& reader,
                           EdictRegistryWireEncoding encoding,
                           std::size_t limit, std::string_view field) {
  std::u16string output;
  while (true) {
    if (reader.empty() ||
        (encoding == EdictRegistryWireEncoding::kUtf16Le &&
         reader.remaining() < 2)) {
      throw EdictRegistryError(std::string(field) + " is unterminated");
    }
    const std::uint16_t unit =
        encoding == EdictRegistryWireEncoding::kAnsiBytes
            ? static_cast<std::uint16_t>(reader.read_u8())
            : reader.read_u16_le();
    if (unit == 0) {
      break;
    }
    if (output.size() >= limit) {
      throw EdictRegistryError(std::string(field) +
                               " exceeds its code-unit limit");
    }
    output.push_back(static_cast<char16_t>(unit));
  }
  if (encoding == EdictRegistryWireEncoding::kUtf16Le) {
    validate_utf16(output, field);
  }
  return output;
}

void apply_flags(std::u16string_view flags, EdictRegistryEntry& entry) {
  for (const char16_t flag : flags) {
    switch (flag) {
      case u'E':
        entry.encoding = EdictRegistryEncoding::kEucJp;
        break;
      case u'U':
        entry.encoding = EdictRegistryEncoding::kUtf8;
        break;
      case u'M':
        entry.encoding = EdictRegistryEncoding::kMixed;
        break;
      case u'N':
        entry.names = EdictRegistryNames::kNames;
        break;
      case u'O':
        entry.names = EdictRegistryNames::kNamesOnly;
        break;
      case u'c':
        entry.special = EdictRegistrySpecial::kClassical;
        break;
      case u'u':
        entry.special = EdictRegistrySpecial::kUser;
        break;
      case u'I':
        entry.indexed = true;
        break;
      case u'B':
        entry.buffered = true;
        break;
      case u'S':
        entry.searched = true;
        break;
      case u'K':
        entry.keep = true;
        break;
      case u'Q':
        entry.quiet = true;
        break;
      default:
        break;
    }
  }
}

std::u16string flags_for(const EdictRegistryEntry& entry) {
  std::u16string flags;
  switch (entry.encoding) {
    case EdictRegistryEncoding::kEucJp:
      flags.push_back(u'E');
      break;
    case EdictRegistryEncoding::kUtf8:
      flags.push_back(u'U');
      break;
    case EdictRegistryEncoding::kMixed:
      flags.push_back(u'M');
      break;
    default:
      throw EdictRegistryError("EDICT registry encoding is invalid");
  }
  switch (entry.names) {
    case EdictRegistryNames::kNone:
      break;
    case EdictRegistryNames::kNames:
      flags.push_back(u'N');
      break;
    case EdictRegistryNames::kNamesOnly:
      flags.push_back(u'O');
      break;
    default:
      throw EdictRegistryError("EDICT registry names role is invalid");
  }
  switch (entry.special) {
    case EdictRegistrySpecial::kNormal:
      break;
    case EdictRegistrySpecial::kClassical:
      flags.push_back(u'c');
      break;
    case EdictRegistrySpecial::kUser:
      flags.push_back(u'u');
      break;
    default:
      throw EdictRegistryError("EDICT registry special role is invalid");
  }
  if (entry.indexed) {
    flags.push_back(u'I');
  }
  if (entry.buffered) {
    flags.push_back(u'B');
  }
  if (entry.searched) {
    flags.push_back(u'S');
  }
  if (entry.keep) {
    flags.push_back(u'K');
  }
  if (entry.quiet) {
    flags.push_back(u'Q');
  }
  return flags;
}

void checked_add(std::size_t& total, std::size_t addition,
                 const EdictRegistryLimits& limits) {
  if (addition > limits.encoded_bytes ||
      total > limits.encoded_bytes - addition) {
    throw EdictRegistryError("EDICT registry exceeds its byte limit");
  }
  total += addition;
}

std::size_t encoded_string_size(std::u16string_view value,
                                EdictRegistryWireEncoding encoding,
                                const EdictRegistryLimits& limits,
                                std::string_view field) {
  if (value.size() > limits.field_code_units) {
    throw EdictRegistryError(std::string(field) +
                             " exceeds its code-unit limit");
  }
  validate_utf16(value, field);
  if (encoding == EdictRegistryWireEncoding::kAnsiBytes) {
    for (const char16_t unit : value) {
      if (static_cast<std::uint16_t>(unit) > 0xffU) {
        throw EdictRegistryError(std::string(field) +
                                 " cannot be represented as ANSI bytes");
      }
    }
    return value.size() + 1;
  }
  if (value.size() >= std::numeric_limits<std::size_t>::max() / 2) {
    throw EdictRegistryError(std::string(field) + " is too large");
  }
  return (value.size() + 1) * 2;
}

void write_string(ByteWriter& writer, std::u16string_view value,
                  EdictRegistryWireEncoding encoding) {
  for (const char16_t unit : value) {
    if (encoding == EdictRegistryWireEncoding::kAnsiBytes) {
      writer.write_u8(static_cast<std::uint8_t>(unit));
    } else {
      writer.write_u16_le(static_cast<std::uint16_t>(unit));
    }
  }
  if (encoding == EdictRegistryWireEncoding::kAnsiBytes) {
    writer.write_u8(0);
  } else {
    writer.write_u16_le(0);
  }
}

}  // namespace

bool EdictRegistryEntry::operator==(
    const EdictRegistryEntry& other) const noexcept {
  return label == other.label && path == other.path &&
         encoding == other.encoding && names == other.names &&
         special == other.special && indexed == other.indexed &&
         buffered == other.buffered && searched == other.searched &&
         keep == other.keep && quiet == other.quiet;
}

bool EdictRegistry::operator==(const EdictRegistry& other) const noexcept {
  return wire_encoding == other.wire_encoding && entries == other.entries;
}

EdictRegistry parse_edict_registry(std::string_view bytes,
                                   const EdictRegistryLimits& limits) {
  validate_limits(limits);
  if (bytes.size() > limits.encoded_bytes || bytes.size() < 8) {
    throw EdictRegistryError("EDICT registry byte size is invalid");
  }

  try {
    ByteReader reader(bytes);
    EdictRegistry registry;
    switch (reader.read_u32_le()) {
      case kAnsiMagic:
        registry.wire_encoding = EdictRegistryWireEncoding::kAnsiBytes;
        break;
      case kUtf16Magic:
        registry.wire_encoding = EdictRegistryWireEncoding::kUtf16Le;
        break;
      default:
        throw EdictRegistryError("EDICT registry magic is invalid");
    }

    while (!at_terminator(bytes, reader.position())) {
      if (registry.entries.size() >= limits.entries) {
        throw EdictRegistryError("EDICT registry contains too many entries");
      }
      const std::u16string flags = read_string(
          reader, registry.wire_encoding, limits.field_code_units,
          "EDICT registry flags");
      if (flags.empty()) {
        throw EdictRegistryError("EDICT registry flags are empty");
      }
      EdictRegistryEntry entry;
      apply_flags(flags, entry);
      entry.label = read_string(reader, registry.wire_encoding,
                                limits.field_code_units,
                                "EDICT registry label");
      entry.path = read_string(reader, registry.wire_encoding,
                               limits.field_code_units,
                               "EDICT registry path");
      registry.entries.push_back(std::move(entry));
    }
    (void)reader.read_u32_le();
    if (!reader.empty()) {
      throw EdictRegistryError("EDICT registry has trailing bytes");
    }
    return registry;
  } catch (const BinaryError& error) {
    throw EdictRegistryError(error.what());
  }
}

std::string serialize_edict_registry(const EdictRegistry& registry,
                                     const EdictRegistryLimits& limits) {
  validate_limits(limits);
  if (registry.entries.size() > limits.entries) {
    throw EdictRegistryError("EDICT registry contains too many entries");
  }
  if (registry.wire_encoding != EdictRegistryWireEncoding::kAnsiBytes &&
      registry.wire_encoding != EdictRegistryWireEncoding::kUtf16Le) {
    throw EdictRegistryError("EDICT registry wire encoding is invalid");
  }

  std::size_t output_size = 8;
  const std::size_t minimum_entry_size =
      registry.wire_encoding == EdictRegistryWireEncoding::kAnsiBytes ? 4 : 8;
  if (registry.entries.size() >
      (limits.encoded_bytes - output_size) / minimum_entry_size) {
    throw EdictRegistryError("EDICT registry exceeds its byte limit");
  }
  std::vector<std::u16string> flags;
  flags.reserve(registry.entries.size());
  for (const EdictRegistryEntry& entry : registry.entries) {
    flags.push_back(flags_for(entry));
    checked_add(output_size,
                encoded_string_size(flags.back(), registry.wire_encoding,
                                    limits, "EDICT registry flags"),
                limits);
    checked_add(output_size,
                encoded_string_size(entry.label, registry.wire_encoding,
                                    limits, "EDICT registry label"),
                limits);
    checked_add(output_size,
                encoded_string_size(entry.path, registry.wire_encoding,
                                    limits, "EDICT registry path"),
                limits);
  }

  ByteWriter writer;
  writer.write_u32_le(registry.wire_encoding ==
                              EdictRegistryWireEncoding::kAnsiBytes
                          ? kAnsiMagic
                          : kUtf16Magic);
  for (std::size_t i = 0; i < registry.entries.size(); ++i) {
    write_string(writer, flags[i], registry.wire_encoding);
    write_string(writer, registry.entries[i].label, registry.wire_encoding);
    write_string(writer, registry.entries[i].path, registry.wire_encoding);
  }
  writer.write_u32_le(0);
  return writer.take_bytes();
}

EdictDictionarySample infer_edict_dictionary_sample(
    std::string_view bytes, LegacyCodePage mixed_code_page,
    std::size_t inspected_bytes) {
  if (bytes.empty()) throw EdictRegistryError("Dictionary sample is empty");
  if (inspected_bytes == 0 || inspected_bytes > 1024U * 1024U)
    throw EdictRegistryError("Dictionary sample limit is invalid");

  const std::size_t size = std::min(inspected_bytes, bytes.size());
  bool ascii = true;
  for (std::size_t i = 0; i < size; ++i)
    if (sample_byte(bytes, i) > 0x7fU) {
      ascii = false;
      break;
    }

  EdictDictionarySample result;
  if (ascii) result.encoding = EdictRegistryEncoding::kEucJp;
  else if (source_utf8_sample(bytes, size))
    result.encoding = EdictRegistryEncoding::kUtf8;
  else if (source_euc_sample(bytes, size))
    result.encoding = EdictRegistryEncoding::kEucJp;
  else result.encoding = EdictRegistryEncoding::kMixed;
  result.description = sample_description(
      bytes, result.encoding, mixed_code_page, size);
  return result;
}

}  // namespace jwpqt::core
