// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_project.h"

#include <utility>

#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/utf16.h"

namespace jwpqt::core {
namespace {

void validate_limits(const JwpProjectLimits& limits) {
  if (limits.encoded_bytes < sizeof(std::uint32_t) + 3U ||
      limits.configuration_bytes == 0 || limits.paths == 0 ||
      limits.path_bytes == 0) {
    throw JwpProjectError("JWP project limits must be positive");
  }
}

std::string read_c_string(ByteReader& reader, std::size_t limit,
                          const char* field) {
  std::string value;
  while (!reader.empty()) {
    const std::uint8_t byte = reader.read_u8();
    if (byte == 0)
      return value;
    if (value.size() >= limit)
      throw JwpProjectError(std::string("JWP project ") + field +
                            " exceeds its byte limit");
    value.push_back(static_cast<char>(byte));
  }
  throw JwpProjectError(std::string("JWP project ") + field +
                        " is not NUL terminated");
}

std::u32string read_path(ByteReader& reader, std::size_t limit,
                         const char* field) {
  std::string bytes;
  while (!reader.empty()) {
    const auto unit = reader.read_u16_le();
    if (unit == 0) {
      try {
        return decode_utf16(bytes, Utf16ByteOrder::kLittleEndian);
      } catch (const Utf16Error& error) {
        throw JwpProjectError(std::string("JWP project ") + field + ": " +
                              error.what());
      }
    }
    if (limit - bytes.size() < 2U)
      throw JwpProjectError(std::string("JWP project ") + field +
                            " exceeds its byte limit");
    bytes.push_back(static_cast<char>(unit & 0xffU));
    bytes.push_back(static_cast<char>(unit >> 8U));
  }
  throw JwpProjectError(std::string("JWP project ") + field +
                        " is not NUL terminated");
}

void validate_field(std::string_view value, std::size_t limit,
                    const char* field, bool may_be_empty) {
  if ((!may_be_empty && value.empty()) || value.size() > limit ||
      value.find('\0') != std::string_view::npos) {
    throw JwpProjectError(std::string("JWP project ") + field +
                          " is invalid");
  }
}

void checked_add(std::size_t& total, std::size_t amount,
                 const JwpProjectLimits& limits) {
  if (amount > limits.encoded_bytes || total > limits.encoded_bytes - amount)
    throw JwpProjectError("JWP project exceeds its byte limit");
  total += amount;
}

}  // namespace

bool JwpProject::operator==(const JwpProject& other) const noexcept {
  return configuration == other.configuration &&
         current_directory == other.current_directory && paths == other.paths;
}

JwpProject parse_jwp_project(std::string_view bytes,
                             const JwpProjectLimits& limits) {
  validate_limits(limits);
  if (bytes.size() > limits.encoded_bytes)
    throw JwpProjectError("JWP project exceeds its byte limit");

  try {
    ByteReader reader(bytes);
    if (reader.read_u32_le() != kJwpProjectMagic)
      throw JwpProjectError("JWP project magic is invalid or unsupported");

    JwpProject project;
    project.configuration = read_c_string(
        reader, limits.configuration_bytes, "configuration");
    project.current_directory =
        read_path(reader, limits.path_bytes, "current directory");
    while (!reader.empty()) {
      if (project.paths.size() >= limits.paths)
        throw JwpProjectError("JWP project contains too many paths");
      std::u32string path = read_path(reader, limits.path_bytes, "path");
      if (path.empty())
        throw JwpProjectError("JWP project contains an empty path");
      project.paths.push_back(std::move(path));
    }
    return project;
  } catch (const BinaryError& error) {
    throw JwpProjectError(std::string("JWP project is truncated: ") +
                          error.what());
  }
}

std::string serialize_jwp_project(const JwpProject& project,
                                  const JwpProjectLimits& limits) {
  validate_limits(limits);
  validate_field(project.configuration, limits.configuration_bytes,
                 "configuration", true);
  if (project.paths.size() > limits.paths)
    throw JwpProjectError("JWP project contains too many paths");

  std::size_t total = sizeof(std::uint32_t);
  checked_add(total, project.configuration.size(), limits);
  checked_add(total, 1U, limits);

  ByteWriter writer;
  writer.write_u32_le(kJwpProjectMagic);
  writer.write_bytes(project.configuration);
  writer.write_u8(0);
  const auto write_path = [&](std::u32string_view path, const char* field,
                              bool may_be_empty) {
    if ((!may_be_empty && path.empty()) ||
        path.find(U'\0') != std::u32string_view::npos ||
        path.size() > limits.path_bytes / 2U)
      throw JwpProjectError(std::string("JWP project ") + field + " is invalid");
    std::string bytes;
    try {
      bytes = encode_utf16(path, Utf16ByteOrder::kLittleEndian);
    } catch (const Utf16Error& error) {
      throw JwpProjectError(std::string("JWP project ") + field + ": " +
                            error.what());
    }
    if (bytes.size() > limits.path_bytes)
      throw JwpProjectError(std::string("JWP project ") + field +
                            " exceeds its byte limit");
    checked_add(total, bytes.size(), limits);
    checked_add(total, 2U, limits);
    writer.write_bytes(bytes);
    writer.write_u16_le(0);
  };
  write_path(project.current_directory, "current directory", true);
  for (const auto& path : project.paths) write_path(path, "path", false);
  return writer.take_bytes();
}

}  // namespace jwpqt::core
