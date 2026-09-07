#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jwpqt::core {

class JwpConfigurationError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct JwpConfigurationLimits {
  std::size_t bytes = 1024U * 1024U;
  std::size_t lines = 4096;
  std::size_t line_bytes = 64U * 1024U;
};

struct JwpConfigurationEntry {
  std::string name;
  std::string value;
  std::size_t line = 0;
  // Source range includes the physical line and its line ending.
  std::size_t begin = 0;
  std::size_t end = 0;
};

struct JwpConfigurationKey {
  std::string name;
  std::string alias;

  bool matches(std::string_view candidate) const noexcept;
};

struct JwpConfigurationUpdate {
  JwpConfigurationKey key;
  std::string value;
};

std::vector<JwpConfigurationEntry> parse_jwp_configuration(
    std::string_view text, const JwpConfigurationLimits& limits = {});
// Raw last-value lookup. A typed overlay must validate every recognized entry
// in source order, including assignments later replaced by another value.
std::optional<std::string> jwp_configuration_value(
    const std::vector<JwpConfigurationEntry>& entries,
    const JwpConfigurationKey& key);
// Unknown lines remain byte-identical. Updated aliases/duplicates are removed
// before appending one canonical assignment per key in source-writer syntax.
std::string rewrite_jwp_configuration(
    std::string_view text, const std::vector<JwpConfigurationUpdate>& updates,
    const JwpConfigurationLimits& limits = {});

bool parse_jwp_setting_bool(std::string_view value);
std::int64_t parse_jwp_setting_integer(std::string_view value,
                                     std::int64_t minimum,
                                     std::int64_t maximum);
std::vector<std::uint8_t> parse_jwp_setting_bytes(std::string_view value,
                                                std::size_t expected_bytes);
// array_units includes the terminating UTF-16 NUL and fixed-size padding.
std::u32string parse_jwp_setting_string(std::string_view value,
                                      std::size_t array_units);
std::string encode_jwp_setting_string(std::u32string_view text,
                                     std::size_t array_units);

}  // namespace jwpqt::core
