// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace jwpqt::core {

inline constexpr std::uint32_t kJwpProjectMagic = 0xf76d6a03U;

class JwpProjectError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct JwpProjectLimits {
  std::size_t encoded_bytes = 8U * 1024U * 1024U;
  std::size_t configuration_bytes = 1024U * 1024U;
  std::size_t paths = 4096;
  std::size_t path_bytes = 64U * 1024U;
};

struct JwpProject {
  // The legacy desktop build stores path strings in the active ANSI code page.
  std::string configuration;
  std::string current_directory;
  std::vector<std::string> paths;

  bool operator==(const JwpProject& other) const noexcept;
};

JwpProject parse_jwp_project(
    std::string_view bytes,
    const JwpProjectLimits& limits = JwpProjectLimits{});
std::string serialize_jwp_project(
    const JwpProject& project,
    const JwpProjectLimits& limits = JwpProjectLimits{});

}  // namespace jwpqt::core
