// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <functional>
#include <initializer_list>
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

std::string prefix(std::string_view configuration = {}) {
  std::string bytes;
  append_u32(bytes, jwpqt::core::kJwpProjectMagic);
  bytes.append(configuration);
  bytes.push_back('\0');
  return bytes;
}

void append_units(std::string& bytes,
                  std::initializer_list<unsigned> units) {
  for (const auto unit : units) {
    bytes.push_back(static_cast<char>(unit & 0xffU));
    bytes.push_back(static_cast<char>(unit >> 8U));
  }
}

void test_parse_and_canonical_round_trip() {
  // project_save writes byte configuration then sizeof(TCHAR) path units.
  std::string bytes = prefix("File.Size = 16\r\n");
  require(bytes.size() % 2 == 1, "Fixture must cover unaligned UTF-16 paths");
  append_units(bytes, {'C', ':', '\\', 0x6587, 0});
  append_units(bytes, {0x4e8c, '.', 'j', 'w', 'p', 0});
  append_units(bytes, {0xfeff, 0xd83d, 0xde00, '.', 'j', 'f', 'c', 0});
  append_units(bytes, {0xfffe, 'x', 0});

  const auto project = jwpqt::core::parse_jwp_project(bytes);
  require(project.configuration == "File.Size = 16\r\n" &&
              project.current_directory == U"C:\\\u6587" &&
              project.paths ==
                  std::vector<std::u32string>{U"\u4e8c.jwp",
                      U"\ufeff\U0001f600.jfc", U"\ufffex"},
          "JWP project fields or path order decoded incorrectly");
  require(jwpqt::core::serialize_jwp_project(project) == bytes,
          "JWP project did not round-trip canonically");

  const jwpqt::core::JwpProject empty{"", U"", {}};
  require(jwpqt::core::parse_jwp_project(
              jwpqt::core::serialize_jwp_project(empty)) == empty,
          "Empty current-format project did not round-trip");
  require(jwpqt::core::serialize_jwp_project(empty).size() == 7,
          "Empty project has the wrong terminator widths");
  for (const auto& configuration : {std::string{}, std::string("# retained\r\n"),
                                    std::string("X"), std::string("opaque\x80")}) {
    const jwpqt::core::JwpProject value{configuration, U"", {U"\u65e5\u672c"}};
    require(jwpqt::core::parse_jwp_project(
                jwpqt::core::serialize_jwp_project(value)) == value,
            "Configuration alignment changed Unicode project paths");
  }
}

void test_rejects_unsupported_or_malformed_projects() {
  std::string wrong;
  append_u32(wrong, 0x832cd37aU);
  wrong.append(3, '\0');
  require_error([&] { (void)jwpqt::core::parse_jwp_project(wrong); },
                "Legacy project magic was accepted as the current format");

  std::string truncated;
  append_u32(truncated, jwpqt::core::kJwpProjectMagic);
  truncated += "configuration without terminator";
  require_error([&] { (void)jwpqt::core::parse_jwp_project(truncated); },
                "Unterminated configuration was accepted");

  std::string empty_path = prefix();
  append_units(empty_path, {'d', 'i', 'r', 0, 0, 'x', 0});
  require_error([&] { (void)jwpqt::core::parse_jwp_project(empty_path); },
                "Embedded empty project path was accepted");
  for (const auto& units : {
           std::initializer_list<unsigned>{0xd800, 0},
           {0xdc00, 0}, {0xd800, 'A', 0}, {0xdc00, 0xd800, 0}}) {
    auto malformed = prefix();
    append_units(malformed, units);
    require_error([&] { (void)jwpqt::core::parse_jwp_project(malformed); },
                  "Malformed UTF-16 current directory was accepted");
    malformed = prefix();
    append_units(malformed, {0});
    append_units(malformed, units);
    require_error([&] { (void)jwpqt::core::parse_jwp_project(malformed); },
                  "Malformed UTF-16 file path was accepted");
  }
  auto truncated_unit = prefix();
  append_units(truncated_unit, {'d'});
  truncated_unit.push_back('\0');
  require_error([&] { (void)jwpqt::core::parse_jwp_project(truncated_unit); },
                "Half of a UTF-16 terminator was accepted");
  auto no_terminator = prefix();
  append_units(no_terminator, {'d'});
  require_error([&] { (void)jwpqt::core::parse_jwp_project(no_terminator); },
                "Unterminated UTF-16 current directory was accepted");
  auto narrow = prefix();
  narrow.append("dir\0file.jwp\0", 13);
  require_error([&] { (void)jwpqt::core::parse_jwp_project(narrow); },
                "Experimental narrow paths were accepted as UTF-16 projects");
  auto extra_end_marker = prefix();
  append_units(extra_end_marker, {0, 'x', 0, 0});
  require_error([&] { (void)jwpqt::core::parse_jwp_project(extra_end_marker); },
                "An extra empty path was accepted as an end marker");
}

void test_limits_and_output_validation() {
  jwpqt::core::JwpProjectLimits limits;
  limits.encoded_bytes = 32;
  limits.configuration_bytes = 4;
  limits.paths = 1;
  limits.path_bytes = 4;
  require_error(
      [&] {
        (void)jwpqt::core::serialize_jwp_project({"12345", U"", {}}, limits);
      },
      "Oversized project configuration was serialized");
  require_error(
      [&] {
        (void)jwpqt::core::serialize_jwp_project(
             {"", U"", {U"a", U"b"}}, limits);
      },
      "Oversized project path count was serialized");
  require_error(
      [&] {
        (void)jwpqt::core::serialize_jwp_project(
             {"", U"", {std::u32string(U"a\0b", 3)}}, limits);
      },
      "NUL-containing project path was serialized");

  std::string bytes = prefix("12345");
  append_units(bytes, {'d', 0});
  require_error([&] { (void)jwpqt::core::parse_jwp_project(bytes, limits); },
                "Oversized parsed configuration was accepted");
  const jwpqt::core::JwpProject boundary{"", U"ab", {U"\U0001f600"}};
  limits.encoded_bytes = 17;
  bytes = jwpqt::core::serialize_jwp_project(boundary, limits);
  require(bytes.size() == 17 &&
              jwpqt::core::parse_jwp_project(bytes, limits) == boundary,
          "Exact UTF-16 payload and overall byte limits were rejected");
  --limits.encoded_bytes;
  require_error([&] { (void)jwpqt::core::serialize_jwp_project(boundary, limits); },
                "Overall serialized byte limit was not enforced");
  require_error([&] { (void)jwpqt::core::parse_jwp_project(bytes, limits); },
                "Overall parsed byte limit was not enforced");
  limits.encoded_bytes = 32;
  limits.path_bytes = 3;
  require_error([&] { (void)jwpqt::core::serialize_jwp_project(boundary, limits); },
                "UTF-16 path limit was measured in scalars instead of bytes");
  require_error([&] { (void)jwpqt::core::parse_jwp_project(bytes, limits); },
                "Parsed UTF-16 path exceeded its byte limit");
  const jwpqt::core::JwpProject supplementary{"", U"", {U"\U0001f600"}};
  require_error([&] { (void)jwpqt::core::serialize_jwp_project(supplementary,
                                                            limits); },
                "A surrogate pair exceeded the serialized path byte limit");
  const auto supplementary_bytes =
      jwpqt::core::serialize_jwp_project(supplementary);
  require_error([&] { (void)jwpqt::core::parse_jwp_project(supplementary_bytes,
                                                       limits); },
                "A surrogate pair exceeded the parsed path byte limit");
  limits.path_bytes = 1;
  const jwpqt::core::JwpProject empty;
  require(jwpqt::core::parse_jwp_project(
              jwpqt::core::serialize_jwp_project(empty, limits), limits) == empty,
          "Tiny payload limit rejected an empty directory");
  const auto two_paths = jwpqt::core::serialize_jwp_project(
      {"", U"", {U"a", U"b"}});
  limits.path_bytes = 4;
  require_error([&] { (void)jwpqt::core::parse_jwp_project(two_paths, limits); },
                "Parsed project path count exceeded its limit");
  for (const auto& invalid : {std::u32string(U"a\0b", 3),
                              std::u32string(1, char32_t{0xd800}),
                              std::u32string(1, char32_t{0xdc00}),
                              std::u32string(1, char32_t{0x110000})}) {
    require_error([&] { (void)jwpqt::core::serialize_jwp_project(
                            {"", invalid, {}}); },
                  "Invalid Unicode current directory was serialized");
    require_error([&] { (void)jwpqt::core::serialize_jwp_project(
                            {"", U"", {invalid}}); },
                  "Invalid Unicode file path was serialized");
  }
  require_error([] { (void)jwpqt::core::serialize_jwp_project(
                          {std::string("a\0b", 3), U"", {}}); },
                "NUL-containing configuration was serialized");
  require_error([] { (void)jwpqt::core::serialize_jwp_project(
                          {"", U"", {U""}}); },
                "Empty file path was serialized");
  limits.encoded_bytes = 6;
  require_error([&] { (void)jwpqt::core::serialize_jwp_project(empty, limits); },
                "Impossible project size limit was accepted");
  for (const auto member : {&jwpqt::core::JwpProjectLimits::encoded_bytes,
                            &jwpqt::core::JwpProjectLimits::configuration_bytes,
                            &jwpqt::core::JwpProjectLimits::paths,
                            &jwpqt::core::JwpProjectLimits::path_bytes}) {
    limits = {};
    limits.*member = 0;
    require_error([&] { (void)jwpqt::core::serialize_jwp_project(empty, limits); },
                  "A zero serialization limit was accepted");
    const auto empty_bytes = jwpqt::core::serialize_jwp_project(empty);
    require_error([&] { (void)jwpqt::core::parse_jwp_project(empty_bytes, limits); },
                  "A zero parsing limit was accepted");
  }
  limits = {};
  jwpqt::core::JwpProject maximum_path{
      "", U"", {std::u32string(limits.path_bytes / 2U, U'x')}};
  bytes = jwpqt::core::serialize_jwp_project(maximum_path);
  require(jwpqt::core::parse_jwp_project(bytes) == maximum_path,
          "The default path byte boundary was rejected");
  maximum_path.paths.front().push_back(U'x');
  require_error([&] { (void)jwpqt::core::serialize_jwp_project(maximum_path); },
                "The default serialized path byte limit was ignored");
  bytes.insert(bytes.size() - 2U, std::string("x\0", 2));
  require_error([&] { (void)jwpqt::core::parse_jwp_project(bytes); },
                "The default parsed path byte limit was ignored");
  jwpqt::core::JwpProject maximum_count;
  maximum_count.paths.assign(limits.paths, U"x");
  bytes = jwpqt::core::serialize_jwp_project(maximum_count);
  require(jwpqt::core::parse_jwp_project(bytes) == maximum_count,
          "The default project path-count boundary was rejected");
  maximum_count.paths.push_back(U"x");
  require_error([&] { (void)jwpqt::core::serialize_jwp_project(maximum_count); },
                "The default serialized path-count limit was ignored");
  append_units(bytes, {'x', 0});
  require_error([&] { (void)jwpqt::core::parse_jwp_project(bytes); },
                "The default parsed path-count limit was ignored");
}

}  // namespace

int main() {
  test_parse_and_canonical_round_trip();
  test_rejects_unsupported_or_malformed_projects();
  test_limits_and_output_validation();
  return EXIT_SUCCESS;
}
