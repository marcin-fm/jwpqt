// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/edict_registry.h"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

namespace {

using jwpqt::core::EdictRegistry;
using jwpqt::core::EdictRegistryEncoding;
using jwpqt::core::EdictRegistryEntry;
using jwpqt::core::EdictRegistryError;
using jwpqt::core::EdictRegistryLimits;
using jwpqt::core::EdictRegistryNames;
using jwpqt::core::EdictRegistrySpecial;
using jwpqt::core::EdictRegistryWireEncoding;
using jwpqt::core::parse_edict_registry;
using jwpqt::core::serialize_edict_registry;

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void require_error(const std::function<void()>& operation,
                   const char* message) {
  try {
    operation();
  } catch (const EdictRegistryError&) {
    return;
  }
  require(false, message);
}

void append_u16_le(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>(value & 0xffU));
  bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

EdictRegistryEntry complete_entry() {
  EdictRegistryEntry entry;
  entry.label = u"Names";
  entry.path = u"dict/enamdict";
  entry.encoding = EdictRegistryEncoding::kUtf8;
  entry.names = EdictRegistryNames::kNames;
  entry.special = EdictRegistrySpecial::kUser;
  entry.indexed = true;
  entry.buffered = true;
  entry.searched = true;
  entry.keep = true;
  entry.quiet = true;
  return entry;
}

void test_ansi_round_trip_and_canonical_flags() {
  EdictRegistry registry;
  registry.wire_encoding = EdictRegistryWireEncoding::kAnsiBytes;
  registry.entries = {complete_entry(), complete_entry()};
  registry.entries[0].label = std::u16string{u'C', u'a', u'f', 0x00e9};
  registry.entries[1].names = EdictRegistryNames::kNamesOnly;
  registry.entries[1].special = EdictRegistrySpecial::kClassical;
  registry.entries[1].searched = false;

  const std::string bytes = serialize_edict_registry(registry);
  require(bytes.size() > 13 &&
              static_cast<unsigned char>(bytes[0]) == 0x76 &&
              static_cast<unsigned char>(bytes[1]) == 0x3e &&
              static_cast<unsigned char>(bytes[2]) == 0xbc &&
              static_cast<unsigned char>(bytes[3]) == 0x12 &&
              bytes.substr(4, 9) == std::string("UNuIBSKQ\0", 9),
          "ANSI registry magic or canonical flag order is wrong");
  require(parse_edict_registry(bytes) == registry,
          "ANSI registry did not preserve entries, bytes, or order");

  registry.entries[0].label = std::u16string{static_cast<char16_t>(0x0080)};
  const std::string opaque_ansi = serialize_edict_registry(registry);
  require(static_cast<unsigned char>(opaque_ansi[13]) == 0x80 &&
              parse_edict_registry(opaque_ansi) == registry,
          "ANSI registry did not preserve an opaque high byte");
}

void test_utf16_round_trip() {
  EdictRegistry registry;
  registry.wire_encoding = EdictRegistryWireEncoding::kUtf16Le;
  EdictRegistryEntry entry = complete_entry();
  entry.label = u"\u8f9e\u66f8";
  entry.path = u"data/\U0001f600.utf";
  registry.entries = {entry};

  const std::string bytes = serialize_edict_registry(registry);
  require(bytes.size() > 4 &&
              static_cast<unsigned char>(bytes[0]) == 0xb7 &&
              static_cast<unsigned char>(bytes[1]) == 0x3e &&
              static_cast<unsigned char>(bytes[2]) == 0xbc &&
              static_cast<unsigned char>(bytes[3]) == 0x52 &&
              parse_edict_registry(bytes) == registry,
          "UTF-16LE registry did not round trip");
}

void test_source_flag_semantics() {
  EdictRegistry registry;
  registry.wire_encoding = EdictRegistryWireEncoding::kAnsiBytes;
  EdictRegistryEntry entry;
  entry.label = u"Defaults";
  entry.path = u"edict";
  registry.entries = {entry};
  std::string bytes = serialize_edict_registry(registry);
  const std::size_t flag_end = bytes.find('\0', 4);
  require(flag_end != std::string::npos, "Could not locate registry flags");
  bytes.replace(4, flag_end - 4, "xEUMNOcuIBSKQ");

  const EdictRegistry parsed = parse_edict_registry(bytes);
  require(parsed.entries.size() == 1 &&
              parsed.entries[0].encoding == EdictRegistryEncoding::kMixed &&
              parsed.entries[0].names == EdictRegistryNames::kNamesOnly &&
              parsed.entries[0].special == EdictRegistrySpecial::kUser &&
              parsed.entries[0].indexed && parsed.entries[0].buffered &&
              parsed.entries[0].searched && parsed.entries[0].keep &&
              parsed.entries[0].quiet,
          "Registry did not ignore unknown flags or apply last recognized roles");
}

void test_empty_registry() {
  EdictRegistry registry;
  registry.wire_encoding = EdictRegistryWireEncoding::kUtf16Le;
  const std::string bytes = serialize_edict_registry(registry);
  require(bytes.size() == 8 && parse_edict_registry(bytes) == registry,
          "Empty registry framing is wrong");

  EdictRegistryEntry empty_fields;
  registry.entries = {empty_fields};
  const std::string entry_bytes = serialize_edict_registry(registry);
  require(parse_edict_registry(entry_bytes) == registry,
          "Empty legacy label or path did not round trip");
  EdictRegistryLimits limits;
  limits.encoded_bytes = entry_bytes.size();
  limits.entries = 1;
  limits.field_code_units = 1;
  require(serialize_edict_registry(registry, limits) == entry_bytes,
          "Exact-size empty-field registry serialization was rejected");
}

void test_malformed_input() {
  require_error([] { (void)parse_edict_registry(std::string(8, '\0')); },
                "Invalid registry magic was accepted");

  EdictRegistry registry;
  registry.wire_encoding = EdictRegistryWireEncoding::kAnsiBytes;
  registry.entries = {complete_entry()};
  std::string bytes = serialize_edict_registry(registry);
  require_error([&] { (void)parse_edict_registry(bytes.substr(0, 5)); },
                "Truncated registry was accepted");
  bytes.push_back('x');
  require_error([&] { (void)parse_edict_registry(bytes); },
                "Trailing registry bytes were accepted");

  registry.wire_encoding = EdictRegistryWireEncoding::kUtf16Le;
  registry.entries[0].label = std::u16string{static_cast<char16_t>(0xd800)};
  require_error([&] { (void)serialize_edict_registry(registry); },
                "Unpaired UTF-16 surrogate was serialized");
  registry.entries[0] = complete_entry();
  bytes = serialize_edict_registry(registry);
  bytes.resize(bytes.size() - 5);
  require_error([&] { (void)parse_edict_registry(bytes); },
                "Odd or unterminated UTF-16 registry was accepted");

  for (const std::uint16_t surrogate : {std::uint16_t{0xd800},
                                        std::uint16_t{0xdc00}}) {
    std::string malformed{"\xb7\x3e\xbc\x52", 4};
    append_u16_le(malformed, u'E');
    append_u16_le(malformed, 0);
    append_u16_le(malformed, surrogate);
    append_u16_le(malformed, 0);
    append_u16_le(malformed, u'p');
    append_u16_le(malformed, 0);
    malformed.append(4, '\0');
    require_error([&] { (void)parse_edict_registry(malformed); },
                  "Malformed raw UTF-16 registry surrogate was accepted");
  }
}

void test_parser_limits() {
  EdictRegistry registry;
  registry.wire_encoding = EdictRegistryWireEncoding::kAnsiBytes;
  EdictRegistryEntry entry;
  entry.label = u"abc";
  entry.path = u"p";
  registry.entries = {entry};
  const std::string bytes = serialize_edict_registry(registry);

  EdictRegistryLimits limits;
  limits.encoded_bytes = bytes.size();
  limits.entries = 1;
  limits.field_code_units = 3;
  require(parse_edict_registry(bytes, limits) == registry,
          "Registry parser rejected exact limits");
  --limits.encoded_bytes;
  require_error([&] { (void)parse_edict_registry(bytes, limits); },
                "Registry parser byte limit was not enforced");
  limits.encoded_bytes = bytes.size();
  limits.field_code_units = 2;
  require_error([&] { (void)parse_edict_registry(bytes, limits); },
                "Registry parser field limit was not enforced");

  registry.entries.push_back(entry);
  const std::string two_entries = serialize_edict_registry(registry);
  limits.encoded_bytes = two_entries.size();
  limits.field_code_units = 3;
  require_error([&] { (void)parse_edict_registry(two_entries, limits); },
                "Registry parser entry limit was not enforced");
}

void test_limits_and_invalid_models() {
  EdictRegistry registry;
  registry.wire_encoding = EdictRegistryWireEncoding::kAnsiBytes;
  registry.entries = {complete_entry()};

  EdictRegistryLimits limits;
  limits.encoded_bytes = 128;
  limits.entries = 1;
  limits.field_code_units = 4;
  require_error([&] { (void)serialize_edict_registry(registry, limits); },
                "Registry field limit was not enforced");

  limits.field_code_units = 64;
  limits.encoded_bytes = 12;
  require_error([&] { (void)serialize_edict_registry(registry, limits); },
                "Registry output byte limit was not enforced");

  registry.entries.push_back(complete_entry());
  limits.encoded_bytes = 1024;
  require_error([&] { (void)serialize_edict_registry(registry, limits); },
                "Registry entry limit was not enforced");

  registry.entries = {complete_entry()};
  registry.entries[0].label = u"\u65e5";
  require_error([&] { (void)serialize_edict_registry(registry); },
                "Non-ANSI registry string was serialized as ANSI");

  registry.entries[0] = complete_entry();
  registry.entries[0].label = std::u16string{u'a', u'\0', u'b'};
  require_error([&] { (void)serialize_edict_registry(registry); },
                "Embedded registry NUL was serialized");
  registry.entries[0] = complete_entry();
  registry.entries[0].encoding = static_cast<EdictRegistryEncoding>(99);
  require_error([&] { (void)serialize_edict_registry(registry); },
                "Invalid registry encoding was serialized");
  registry.entries[0] = complete_entry();
  registry.entries[0].names = static_cast<EdictRegistryNames>(99);
  require_error([&] { (void)serialize_edict_registry(registry); },
                "Invalid registry names role was serialized");
  registry.entries[0] = complete_entry();
  registry.entries[0].special = static_cast<EdictRegistrySpecial>(99);
  require_error([&] { (void)serialize_edict_registry(registry); },
                "Invalid registry special role was serialized");
  registry.entries[0] = complete_entry();
  registry.wire_encoding = static_cast<EdictRegistryWireEncoding>(99);
  require_error([&] { (void)serialize_edict_registry(registry); },
                "Invalid registry wire encoding was serialized");
}

}  // namespace

int main() {
  test_ansi_round_trip_and_canonical_flags();
  test_utf16_round_trip();
  test_source_flag_semantics();
  test_empty_registry();
  test_malformed_input();
  test_parser_limits();
  test_limits_and_invalid_models();
  return EXIT_SUCCESS;
}
