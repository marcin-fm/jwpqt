// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_resources.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

#include <QDir>
#include <QFileInfo>

#include "jwpqt/core/legacy_code_page.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

std::runtime_error resource_error(const QString& action, const QString& path,
                                  const QString& detail) {
  return std::runtime_error(
      QStringLiteral("%1 %2: %3").arg(action, path, detail).toUtf8().toStdString());
}

class FileDescriptor {
 public:
  explicit FileDescriptor(int value) : value_(value) {}
  ~FileDescriptor() {
    if (value_ >= 0) {
      ::close(value_);
    }
  }

  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor& operator=(const FileDescriptor&) = delete;

  int get() const noexcept { return value_; }

 private:
  int value_;
};

QString system_error(int error) {
  return QString::fromLocal8Bit(std::strerror(error));
}

std::string read_bounded_file(const QString& path, std::size_t limit) {
  if (limit == 0 ||
      limit >= static_cast<std::size_t>(std::numeric_limits<qint64>::max())) {
    throw std::runtime_error("Dictionary file byte limit is invalid");
  }

  const QByteArray native_path = QFile::encodeName(path);
  const int descriptor =
      ::open(native_path.constData(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
  if (descriptor < 0) {
    const int error = errno;
    throw resource_error(QStringLiteral("Could not open"), path,
                         system_error(error));
  }
  const FileDescriptor input(descriptor);

  struct stat status {};
  if (::fstat(input.get(), &status) != 0) {
    const int error = errno;
    throw resource_error(QStringLiteral("Could not inspect"), path,
                         system_error(error));
  }
  if (!S_ISREG(status.st_mode)) {
    throw resource_error(QStringLiteral("Could not read"), path,
                         QStringLiteral("path is not a regular file"));
  }

  std::string bytes;
  bytes.reserve(std::min<std::size_t>(limit, 64U * 1024U));
  while (true) {
    const std::size_t remaining = limit - bytes.size();
    std::array<char, 64U * 1024U> buffer{};
    const std::size_t request = std::min<std::size_t>(remaining + 1,
                                                      buffer.size());
    const ssize_t size = ::read(input.get(), buffer.data(), request);
    if (size < 0) {
      const int error = errno;
      if (error == EINTR) {
        continue;
      }
      throw resource_error(QStringLiteral("Could not read"), path,
                           system_error(error));
    }
    if (size == 0) {
      break;
    }
    bytes.append(buffer.data(), static_cast<std::size_t>(size));
    if (bytes.size() > limit) {
      throw resource_error(QStringLiteral("Could not read"), path,
                           QStringLiteral("file exceeds its byte limit"));
    }
  }
  return bytes;
}

QString decode_ansi_field(std::u16string_view field,
                          core::LegacyCodePage code_page) {
  std::u32string decoded;
  decoded.reserve(field.size());
  for (const char16_t unit : field) {
    if (unit > 0xffU) {
      throw std::runtime_error(
          "ANSI dictionary registry field contains a non-byte code unit");
    }
    const auto code_point = core::legacy_byte_to_unicode(
        static_cast<std::uint8_t>(unit), code_page);
    if (!code_point.has_value()) {
      throw std::runtime_error(
          "ANSI dictionary registry field contains an undefined byte");
    }
    decoded.push_back(*code_point);
  }
  return to_qstring(decoded);
}

QString decode_registry_field(const core::EdictRegistry& registry,
                              std::u16string_view field,
                              core::LegacyCodePage ansi_code_page) {
  switch (registry.wire_encoding) {
    case core::EdictRegistryWireEncoding::kAnsiBytes:
      return decode_ansi_field(field, ansi_code_page);
    case core::EdictRegistryWireEncoding::kUtf16Le:
      return QString::fromStdU16String(std::u16string(field));
  }
  throw std::runtime_error("Dictionary registry wire encoding is invalid");
}

core::EdictEncoding dictionary_encoding(core::EdictRegistryEncoding encoding) {
  switch (encoding) {
    case core::EdictRegistryEncoding::kEucJp:
      return core::EdictEncoding::kEucJp;
    case core::EdictRegistryEncoding::kUtf8:
      return core::EdictEncoding::kUtf8;
    case core::EdictRegistryEncoding::kMixed:
      return core::EdictEncoding::kMixed;
  }
  throw std::runtime_error("Dictionary registry format is invalid");
}

QString resolve_path(const QString& path, const QString& config_directory) {
  if (path.isEmpty()) {
    throw std::runtime_error("Dictionary path is empty");
  }
  if (QDir::isAbsolutePath(path)) {
    return QDir::cleanPath(path);
  }
  if (config_directory.isEmpty()) {
    throw std::runtime_error(
        "Relative dictionary path has no configuration directory");
  }
  return QDir::cleanPath(QDir(config_directory).absoluteFilePath(path));
}

QString exception_message(const std::exception& error) {
  return QString::fromUtf8(error.what());
}

bool valid_code_page(core::LegacyCodePage code_page) {
  return core::parse_legacy_code_page(core::legacy_code_page_name(code_page)) ==
         code_page;
}

void validate_load_options(const EdictResourceLoadOptions& options) {
  if (!valid_code_page(options.ansi_code_page) ||
      !valid_code_page(options.index_options.utf8_code_page)) {
    throw std::runtime_error("Dictionary resource code page is invalid");
  }
  if (options.registry_limits.encoded_bytes == 0 ||
      options.registry_limits.entries == 0 ||
      options.registry_limits.field_code_units == 0 ||
      options.dictionary_limits.encoded_bytes == 0 ||
      options.dictionary_limits.line_bytes == 0 ||
      options.dictionary_limits.records == 0 ||
      options.dictionary_limits.definitions == 0 ||
      options.dictionary_limits.decoded_code_points == 0 ||
      options.index_options.encoded_bytes == 0 ||
      options.index_options.entries == 0 ||
      options.index_options.lookup_steps == 0 ||
      options.index_options.matches == 0 || options.resource_entries == 0 ||
      options.dictionary_bytes == 0 ||
      options.index_bytes == 0 || options.dictionary_records == 0 ||
      options.definitions == 0 || options.decoded_code_points == 0 ||
      options.index_entries == 0) {
    throw std::runtime_error("Dictionary aggregate limits must be positive");
  }
}

std::size_t remaining(std::size_t used, std::size_t limit,
                      std::string_view resource) {
  if (used >= limit) {
    throw std::runtime_error(std::string(resource) +
                             " exceeds its aggregate limit");
  }
  return limit - used;
}

void charge(std::size_t& used, std::size_t amount, std::size_t limit,
            std::string_view resource) {
  if (amount > limit || used > limit - amount) {
    throw std::runtime_error(std::string(resource) +
                             " exceeds its aggregate limit");
  }
  used += amount;
}

struct DictionaryReservation {
  std::size_t records = 0;
  std::size_t definitions = 0;
  std::size_t decoded_code_points = 0;
};

DictionaryReservation failed_parse_reservation(std::string_view bytes) {
  DictionaryReservation reservation;
  reservation.decoded_code_points = bytes.size();
  std::size_t start = 0;
  while (start < bytes.size()) {
    std::size_t end = start;
    while (end < bytes.size() && bytes[end] != '\r' && bytes[end] != '\n') {
      ++end;
    }
    if (end == bytes.size()) {
      break;
    }
    ++reservation.records;
    const std::size_t slashes = static_cast<std::size_t>(
        std::count(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                   bytes.begin() + static_cast<std::ptrdiff_t>(end), '/'));
    if (slashes > 0) {
      charge(reservation.definitions, slashes - 1,
             std::numeric_limits<std::size_t>::max(),
             "Dictionary definitions");
    }
    const char first_break = bytes[end++];
    if (end < bytes.size() && bytes[end] != first_break &&
        (bytes[end] == '\r' || bytes[end] == '\n')) {
      ++end;
    }
    start = end;
  }
  return reservation;
}

std::size_t potential_index_entries(std::size_t bytes) {
  if (bytes <= sizeof(std::uint32_t)) {
    return 0;
  }
  const std::size_t payload = bytes - sizeof(std::uint32_t);
  return payload / sizeof(std::uint32_t) +
         (payload % sizeof(std::uint32_t) != 0 ? 1U : 0U);
}

}  // namespace

QString edict_index_path(const QString& source_path) {
  const QFileInfo info(source_path);
  const QString name = info.fileName();
  const qsizetype separator = name.lastIndexOf(QLatin1Char('.'));
  const QString index_name =
      separator >= 0 ? name.left(separator) + QStringLiteral(".jdx")
                     : name + QStringLiteral(".jdx");
  return QDir::cleanPath(info.dir().filePath(index_name));
}

EdictResourceSet load_edict_resources(
    const core::EdictRegistry& registry, const QString& config_directory,
    const EdictResourceLoadOptions& options) {
  static_cast<void>(
      core::serialize_edict_registry(registry, options.registry_limits));
  validate_load_options(options);

  EdictResourceSet result;
  result.registry = registry;
  const std::size_t reserved_entries =
      std::min(registry.entries.size(), options.resource_entries);
  result.resources.reserve(reserved_entries);
  result.failures.reserve(reserved_entries);

  std::size_t attempted_resources = 0;
  std::size_t used_dictionary_bytes = 0;
  std::size_t used_index_bytes = 0;
  std::size_t used_records = 0;
  std::size_t used_definitions = 0;
  std::size_t used_code_points = 0;
  std::size_t used_index_entries = 0;
  for (std::size_t i = 0; i < registry.entries.size(); ++i) {
    const core::EdictRegistryEntry& entry = registry.entries[i];
    if (!entry.searched) {
      continue;
    }
    if (attempted_resources == options.resource_entries) {
      result.truncated = true;
      break;
    }

    QString source_path;
    try {
      ++attempted_resources;
      source_path = resolve_path(
          decode_registry_field(registry, entry.path, options.ansi_code_page),
          config_directory);
      const QString label = decode_registry_field(
          registry, entry.label, options.ansi_code_page);
      const std::size_t dictionary_bytes_remaining =
          remaining(used_dictionary_bytes, options.dictionary_bytes,
                    "Dictionary source bytes");
      core::EdictParseLimits dictionary_limits = options.dictionary_limits;
      dictionary_limits.encoded_bytes = std::min(
          dictionary_limits.encoded_bytes, dictionary_bytes_remaining);
      dictionary_limits.records =
          std::min(dictionary_limits.records,
                   remaining(used_records, options.dictionary_records,
                             "Dictionary records"));
      dictionary_limits.definitions =
          std::min(dictionary_limits.definitions,
                   remaining(used_definitions, options.definitions,
                             "Dictionary definitions"));
      dictionary_limits.decoded_code_points =
          std::min(dictionary_limits.decoded_code_points,
                   remaining(used_code_points, options.decoded_code_points,
                             "Dictionary decoded text"));
      const std::string source_bytes = read_bounded_file(
          source_path, dictionary_limits.encoded_bytes);
      charge(used_dictionary_bytes, source_bytes.size(),
             options.dictionary_bytes, "Dictionary source bytes");
      core::EdictDictionary dictionary;
      try {
        dictionary = core::EdictDictionary::parse(
            source_bytes, dictionary_encoding(entry.encoding),
            dictionary_limits, options.mixed_code_page,
            entry.special == core::EdictRegistrySpecial::kClassical &&
                entry.encoding == core::EdictRegistryEncoding::kEucJp &&
                !entry.indexed);
      } catch (const std::bad_alloc&) {
        throw;
      } catch (const std::exception&) {
        const DictionaryReservation reservation =
            failed_parse_reservation(source_bytes);
        charge(used_records,
               std::min(reservation.records, dictionary_limits.records),
               options.dictionary_records, "Dictionary records");
        charge(used_definitions,
               std::min(reservation.definitions,
                        dictionary_limits.definitions),
               options.definitions, "Dictionary definitions");
        charge(used_code_points,
               std::min(reservation.decoded_code_points,
                        dictionary_limits.decoded_code_points),
               options.decoded_code_points, "Dictionary decoded text");
        throw;
      }
      charge(used_records,
             dictionary.records().size() + dictionary.record_errors().size(),
             options.dictionary_records, "Dictionary records");
      charge(used_definitions, dictionary.definition_count(),
             options.definitions, "Dictionary definitions");
      charge(used_code_points, dictionary.decoded_code_points(),
             options.decoded_code_points, "Dictionary decoded text");
      if (dictionary.records().empty() && !dictionary.record_errors().empty()) {
        throw core::EdictDictionaryError("Dictionary has no valid records");
      }

      std::optional<QString> index_path;
      std::optional<core::EdictIndex> index;
      if (entry.indexed) {
        index_path = edict_index_path(source_path);
        const std::size_t index_bytes_remaining =
            remaining(used_index_bytes, options.index_bytes,
                      "Dictionary index bytes");
        core::EdictIndexOptions index_options = options.index_options;
        index_options.encoded_bytes =
            std::min(index_options.encoded_bytes, index_bytes_remaining);
        index_options.entries =
            std::min(index_options.entries,
                     remaining(used_index_entries, options.index_entries,
                               "Dictionary index entries"));
        const std::string index_bytes = read_bounded_file(
            *index_path, index_options.encoded_bytes);
        charge(used_index_bytes, index_bytes.size(), options.index_bytes,
               "Dictionary index bytes");
        try {
          index =
              core::EdictIndex::parse(index_bytes, dictionary, index_options);
        } catch (const std::bad_alloc&) {
          throw;
        } catch (const std::exception&) {
          charge(used_index_entries,
                 std::min(potential_index_entries(index_bytes.size()),
                          index_options.entries),
                 options.index_entries, "Dictionary index entries");
          throw;
        }
        charge(used_index_entries, index->entries().size(),
               options.index_entries, "Dictionary index entries");
      }

      result.resources.push_back(EdictLoadedResource{
          i, entry, label, source_path, std::move(index_path),
          std::move(dictionary), std::move(index)});
    } catch (const std::bad_alloc&) {
      throw;
    } catch (const std::exception& error) {
      result.failures.push_back(
          EdictResourceFailure{i, source_path, exception_message(error),
                               entry.quiet});
    }
  }
  return result;
}

}  // namespace jwpqt::qt
