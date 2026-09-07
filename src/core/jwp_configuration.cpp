#include "jwpqt/core/jwp_configuration.h"

#include <charconv>
#include <limits>
#include <unordered_map>

#include "jwpqt/core/utf16.h"

namespace jwpqt::core {
namespace {

char lower_ascii(char value) noexcept {
  return value >= 'A' && value <= 'Z'
             ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool equal_name(std::string_view left, std::string_view right) noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (lower_ascii(left[i]) != lower_ascii(right[i])) return false;
  }
  return true;
}

std::string folded_name(std::string_view name) {
  std::string result(name);
  for (char& value : result) value = lower_ascii(value);
  return result;
}

bool name_start(char value) noexcept {
  return (value >= 'A' && value <= 'Z') ||
         (value >= 'a' && value <= 'z') || value == '_';
}

bool valid_key(std::string_view name) noexcept {
  if (name.empty() || name.size() > 128 || !name_start(name.front())) return false;
  for (char value : name) {
    if (!name_start(value) && !(value >= '0' && value <= '9') && value != '.') {
      return false;
    }
  }
  return true;
}

std::string_view trim(std::string_view value) noexcept {
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string_view::npos) return {};
  const auto last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

int hex_digit(char value) noexcept {
  if (value >= '0' && value <= '9') return value - '0';
  value = lower_ascii(value);
  return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

void check_string_units(std::size_t units) {
  if (units == 0 || units > 32768) {
    throw JwpConfigurationError("Invalid configuration string array size");
  }
}

}  // namespace

bool JwpConfigurationKey::matches(std::string_view candidate) const noexcept {
  return equal_name(name, candidate) || (!alias.empty() && alias == candidate);
}

std::vector<JwpConfigurationEntry> parse_jwp_configuration(
    std::string_view text, const JwpConfigurationLimits& limits) {
  if (limits.bytes == 0 || limits.lines == 0 || limits.line_bytes == 0) {
    throw JwpConfigurationError("Configuration limits must be positive");
  }
  if (text.size() > limits.bytes || text.find('\0') != std::string_view::npos) {
    throw JwpConfigurationError("Configuration is too large or contains NUL");
  }
  std::vector<JwpConfigurationEntry> result;
  std::size_t begin = 0;
  std::size_t line = 0;
  while (begin < text.size()) {
    if (++line > limits.lines) {
      throw JwpConfigurationError("Configuration exceeds the line limit");
    }
    auto end = text.find_first_of("\r\n", begin);
    if (end == std::string_view::npos) end = text.size();
    if (end - begin > limits.line_bytes) {
      throw JwpConfigurationError("Configuration line is too long");
    }
    auto next = end;
    if (next < text.size() && text[next++] == '\r' &&
        next < text.size() && text[next] == '\n') ++next;
    const auto content = trim(text.substr(begin, end - begin));
    if (!content.empty() && name_start(content.front())) {
      const auto name_end = content.find_first_of("= \t");
      if (name_end == std::string_view::npos) {
        throw JwpConfigurationError("Missing setting value on line " +
                                    std::to_string(line));
      }
      const auto value_begin = content.find_first_not_of("= \t", name_end);
      if (value_begin == std::string_view::npos) {
        throw JwpConfigurationError("Missing setting value on line " +
                                    std::to_string(line));
      }
      result.push_back({std::string(content.substr(0, name_end)),
                        std::string(content.substr(value_begin)), line, begin, next});
    }
    begin = next;
  }
  return result;
}

std::optional<std::string> jwp_configuration_value(
    const std::vector<JwpConfigurationEntry>& entries,
    const JwpConfigurationKey& key) {
  for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
    if (key.matches(entry->name)) return entry->value;
  }
  return std::nullopt;
}

std::string rewrite_jwp_configuration(
    std::string_view text, const std::vector<JwpConfigurationUpdate>& updates,
    const JwpConfigurationLimits& limits) {
  const auto entries = parse_jwp_configuration(text, limits);
  if (updates.size() > limits.lines) {
    throw JwpConfigurationError("Too many configuration updates");
  }
  std::unordered_map<std::string, std::size_t> names;
  std::unordered_map<std::string, std::size_t> aliases;
  for (std::size_t i = 0; i < updates.size(); ++i) {
    const auto& update = updates[i];
    if (!valid_key(update.key.name) ||
        (!update.key.alias.empty() && !valid_key(update.key.alias)) ||
        trim(update.value).empty() ||
        update.value.find_first_of("\r\n\0", 0, 3) != std::string::npos ||
        !names.emplace(folded_name(update.key.name), i).second) {
      throw JwpConfigurationError("Invalid or duplicate configuration update");
    }
  }
  for (std::size_t i = 0; i < updates.size(); ++i) {
    const auto& alias = updates[i].key.alias;
    if (alias.empty()) continue;
    const auto name = names.find(folded_name(alias));
    if ((name != names.end() && name->second != i) ||
        !aliases.emplace(alias, i).second) {
      throw JwpConfigurationError("Ambiguous configuration update alias");
    }
  }
  std::string result;
  const auto append = [&](std::string_view value) {
    if (value.size() > limits.bytes - result.size()) {
      throw JwpConfigurationError("Updated configuration exceeds the byte limit");
    }
    result.append(value);
  };
  std::size_t begin = 0;
  for (const auto& entry : entries) {
    if (names.count(folded_name(entry.name)) == 0 && aliases.count(entry.name) == 0) {
      continue;
    }
    append(text.substr(begin, entry.begin - begin));
    begin = entry.end;
  }
  append(text.substr(begin));
  if (!updates.empty() && !result.empty() &&
      result.back() != '\n' && result.back() != '\r') append("\r\n");
  for (const auto& update : updates) {
    append(update.key.name);
    append(" = ");
    append(update.value);
    append("\r\n");
  }
  (void)parse_jwp_configuration(result, limits);
  return result;
}

std::int64_t parse_jwp_setting_integer(std::string_view value,
                                     std::int64_t minimum,
                                     std::int64_t maximum) {
  value = trim(value);
  if (minimum > maximum || value.empty()) {
    throw JwpConfigurationError("Invalid configuration integer range or value");
  }
  bool negative = false;
  if (value.front() == '-' || value.front() == '+') {
    negative = value.front() == '-';
    value.remove_prefix(1);
  }
  int base = 10;
  if (value.size() >= 2 && value[0] == '0' && lower_ascii(value[1]) == 'x') {
    value.remove_prefix(2);
    base = 16;
  }
  if (value.empty()) throw JwpConfigurationError("Missing configuration integer");
  std::uint64_t magnitude = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                      magnitude, base);
  const auto signed_max = static_cast<std::uint64_t>(
      std::numeric_limits<std::int64_t>::max());
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
      magnitude > signed_max + (negative ? 1U : 0U)) {
    throw JwpConfigurationError("Invalid or overflowing configuration integer");
  }
  const std::int64_t result = negative
      ? (magnitude == signed_max + 1U ? std::numeric_limits<std::int64_t>::min()
                                      : -static_cast<std::int64_t>(magnitude))
      : static_cast<std::int64_t>(magnitude);
  if (result < minimum || result > maximum) {
    throw JwpConfigurationError("Configuration integer is outside its field range");
  }
  return result;
}

bool parse_jwp_setting_bool(std::string_view value) {
  value = trim(value);
  if (equal_name(value, "true") || equal_name(value, "yes") ||
      equal_name(value, "t") || equal_name(value, "y")) return true;
  if (equal_name(value, "false") || equal_name(value, "no") ||
      equal_name(value, "f") || equal_name(value, "n")) return false;
  return parse_jwp_setting_integer(value, 0, 1) != 0;
}

std::vector<std::uint8_t> parse_jwp_setting_bytes(std::string_view value,
                                                std::size_t expected_bytes) {
  if (expected_bytes > 64U * 1024U || value.size() > 256U * 1024U) {
    throw JwpConfigurationError("Configuration byte array is too large");
  }
  std::vector<std::uint8_t> result;
  std::size_t position = 0;
  while (position < value.size()) {
    const char current = value[position];
    if (current == ' ' || current == '\t' || current == ',' || current == '$') {
      ++position;
      continue;
    }
    if (position + 1 < value.size() && current == '0' &&
        lower_ascii(value[position + 1]) == 'x') {
      position += 2;
      continue;
    }
    const auto begin = position;
    while (position < value.size() && hex_digit(value[position]) >= 0) ++position;
    if (begin == position) throw JwpConfigurationError("Invalid configuration byte run");
    auto digit = begin;
    // The source treats an odd run's first nibble as a complete byte.
    while (digit < position) {
      if (result.size() == expected_bytes) {
        throw JwpConfigurationError("Configuration byte array has the wrong size");
      }
      int byte = hex_digit(value[digit++]);
      if ((position - digit) % 2 != 0) byte = byte * 16 + hex_digit(value[digit++]);
      result.push_back(static_cast<std::uint8_t>(byte));
    }
  }
  if (result.size() != expected_bytes) {
    throw JwpConfigurationError("Configuration byte array has the wrong size");
  }
  return result;
}

std::u32string parse_jwp_setting_string(std::string_view value,
                                      std::size_t array_units) {
  check_string_units(array_units);
  value = trim(value);
  if (!value.empty() && value.front() == '"') {
    const auto end = value.find('"', 1);
    if (end == std::string_view::npos || !trim(value.substr(end + 1)).empty() ||
        end > array_units) {
      throw JwpConfigurationError("Invalid quoted configuration string");
    }
    std::u32string result;
    for (char byte : value.substr(1, end - 1)) {
      if (byte < 0x20 || byte > 0x7e) {
        throw JwpConfigurationError("Quoted configuration strings must be ASCII");
      }
      result.push_back(static_cast<char32_t>(byte));
    }
    return result;
  }
  const auto bytes = parse_jwp_setting_bytes(value, array_units * 2);
  std::string payload;
  bool terminated = false;
  // The unused tail of a fixed C string can retain an earlier value.
  for (std::size_t i = 0; i < bytes.size(); i += 2) {
    if (bytes[i] == 0 && bytes[i + 1] == 0) {
      terminated = true;
      break;
    }
    payload.push_back(static_cast<char>(bytes[i]));
    payload.push_back(static_cast<char>(bytes[i + 1]));
  }
  if (!terminated) throw JwpConfigurationError("Unterminated configuration string array");
  try {
    return decode_utf16(payload, Utf16ByteOrder::kLittleEndian);
  } catch (const Utf16Error& error) {
    throw JwpConfigurationError(error.what());
  }
}

std::string encode_jwp_setting_string(std::u32string_view text,
                                     std::size_t array_units) {
  check_string_units(array_units);
  if (text.size() >= array_units || text.find(U'\0') != std::u32string_view::npos) {
    throw JwpConfigurationError("Configuration string is too long or contains NUL");
  }
  std::string bytes;
  try {
    bytes = encode_utf16(text, Utf16ByteOrder::kLittleEndian);
  } catch (const Utf16Error& error) {
    throw JwpConfigurationError(error.what());
  }
  if (bytes.size() / 2 >= array_units) {
    throw JwpConfigurationError("Configuration string exceeds its UTF-16 field");
  }
  bool printable = true;
  for (char32_t value : text) {
    printable = printable && value >= 0x20 && value <= 0x7e && value != U'"';
  }
  if (printable) {
    std::string result = "\"";
    for (char32_t value : text) result.push_back(static_cast<char>(value));
    result += '"';
    return result;
  }
  bytes.resize(array_units * 2, '\0');
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (char value : bytes) {
    const auto byte = static_cast<unsigned char>(value);
    result += hex[byte >> 4U];
    result += hex[byte & 0xfU];
  }
  return result;
}

}  // namespace jwpqt::core
