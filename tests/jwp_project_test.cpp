// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

#include "jwpqt/core/jwp_project.h"

namespace {

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
  } catch (const jwpqt::core::JwpProjectError&) {
    return;
  }
  require(false, message);
}

void append_u32(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
}

void test_parse_and_canonical_round_trip() {
  std::string bytes;
  append_u32(bytes, jwpqt::core::kJwpProjectMagic);
  bytes += "Font_Size = 16\r\n";
  bytes.push_back('\0');
  bytes += "C:\\Documents";
  bytes.push_back('\0');
  bytes += "second.jwp";
  bytes.push_back('\0');
  bytes += "first.jfc";
  bytes.push_back('\0');

  const auto project = jwpqt::core::parse_jwp_project(bytes);
  require(project.configuration == "Font_Size = 16\r\n" &&
              project.current_directory == "C:\\Documents" &&
              project.paths ==
                  std::vector<std::string>{"second.jwp", "first.jfc"},
          "JWP project fields or path order decoded incorrectly");
  require(jwpqt::core::serialize_jwp_project(project) == bytes,
          "JWP project did not round-trip canonically");

  const jwpqt::core::JwpProject empty{"", "", {}};
  require(jwpqt::core::parse_jwp_project(
              jwpqt::core::serialize_jwp_project(empty)) == empty,
          "Empty current-format project did not round-trip");
}

void test_rejects_unsupported_or_malformed_projects() {
  std::string wrong;
  append_u32(wrong, 0x832cd37aU);
  wrong += "\0\0";
  require_error([&] { (void)jwpqt::core::parse_jwp_project(wrong); },
                "Legacy project magic was accepted as the current format");

  std::string truncated;
  append_u32(truncated, jwpqt::core::kJwpProjectMagic);
  truncated += "configuration without terminator";
  require_error([&] { (void)jwpqt::core::parse_jwp_project(truncated); },
                "Unterminated configuration was accepted");

  std::string empty_path;
  append_u32(empty_path, jwpqt::core::kJwpProjectMagic);
  empty_path += "\0dir\0\0trailing\0";
  require_error([&] { (void)jwpqt::core::parse_jwp_project(empty_path); },
                "Embedded empty project path was accepted");
}

void test_limits_and_output_validation() {
  jwpqt::core::JwpProjectLimits limits;
  limits.encoded_bytes = 32;
  limits.configuration_bytes = 4;
  limits.paths = 1;
  limits.path_bytes = 4;
  require_error(
      [&] {
        (void)jwpqt::core::serialize_jwp_project({"12345", "", {}}, limits);
      },
      "Oversized project configuration was serialized");
  require_error(
      [&] {
        (void)jwpqt::core::serialize_jwp_project(
            {"", "dir", {"one", "two"}}, limits);
      },
      "Oversized project path count was serialized");
  require_error(
      [&] {
        (void)jwpqt::core::serialize_jwp_project(
            {"", "dir", {std::string("a\0b", 3)}}, limits);
      },
      "NUL-containing project path was serialized");

  std::string bytes;
  append_u32(bytes, jwpqt::core::kJwpProjectMagic);
  bytes += "12345\0d\0";
  require_error([&] { (void)jwpqt::core::parse_jwp_project(bytes, limits); },
                "Oversized parsed configuration was accepted");
}

}  // namespace

int main() {
  test_parse_and_canonical_round_trip();
  test_rejects_unsupported_or_malformed_projects();
  test_limits_and_output_validation();
  return EXIT_SUCCESS;
}
