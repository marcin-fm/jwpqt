// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/stat.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "edict_resources.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void write_bytes(const QString& path, std::string_view bytes) {
  QFile output(path);
  require(output.open(QIODevice::WriteOnly), "Could not create resource fixture");
  require(output.write(bytes.data(), static_cast<qint64>(bytes.size())) ==
              static_cast<qint64>(bytes.size()),
          "Could not write resource fixture");
}

void append_le32(std::string& bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
  }
}

std::string single_entry_index(std::string_view source) {
  std::string bytes;
  append_le32(bytes, static_cast<std::uint32_t>(source.size()));
  append_le32(bytes, 1);
  return bytes;
}

jwpqt::core::EdictRegistryEntry entry(std::u16string path, bool indexed,
                                      bool quiet = false) {
  jwpqt::core::EdictRegistryEntry result;
  result.label = u"Dictionary";
  result.path = std::move(path);
  result.indexed = indexed;
  result.searched = true;
  result.quiet = quiet;
  return result;
}

void test_index_path_derivation() {
  require(jwpqt::qt::edict_index_path(QStringLiteral("/tmp/main.dic")) ==
              QStringLiteral("/tmp/main.jdx") &&
              jwpqt::qt::edict_index_path(QStringLiteral("/tmp/main")) ==
                  QStringLiteral("/tmp/main.jdx") &&
              jwpqt::qt::edict_index_path(QStringLiteral("/tmp/.edict")) ==
                  QStringLiteral("/tmp/.jdx"),
          "JDX path derivation is wrong");
}

void test_ordered_resource_loading(const QString& directory) {
  const std::string first = "abc /first/\n";
  const std::string second = "def /second/\n";
  write_bytes(directory + QStringLiteral("/first.dic"), first);
  write_bytes(directory + QStringLiteral("/first.jdx"),
              single_entry_index(first));
  write_bytes(directory + QStringLiteral("/second.utf"), second);

  jwpqt::core::EdictRegistry registry;
  registry.entries = {
      entry(u"first.dic", true),
      entry(u"disabled.dic", false),
      entry(u"missing.dic", true, true),
      entry((directory + QStringLiteral("/second.utf")).toStdU16String(),
            false),
  };
  registry.entries[1].searched = false;
  registry.entries[3].encoding =
      jwpqt::core::EdictRegistryEncoding::kUtf8;

  const jwpqt::qt::EdictResourceSet loaded =
      jwpqt::qt::load_edict_resources(registry, directory);
  require(loaded.registry == registry && !loaded.truncated &&
              loaded.resources.size() == 2 &&
              loaded.resources[0].registry_index == 0 &&
              loaded.resources[1].registry_index == 3,
          "Resource loading did not preserve registry order and state");
  require(loaded.resources[0].source_path ==
              directory + QStringLiteral("/first.dic") &&
              loaded.resources[0].index_path ==
                  directory + QStringLiteral("/first.jdx") &&
              loaded.resources[0].index.has_value() &&
              !loaded.resources[1].index.has_value(),
          "Indexed and unindexed resources loaded incorrectly");
  require(loaded.failures.size() == 1 &&
              loaded.failures[0].registry_index == 2 &&
              loaded.failures[0].quiet,
          "Quiet resource failure was not retained");
}

void test_failure_isolation_and_limits(const QString& directory) {
  const std::string good = "good /entry/\n";
  write_bytes(directory + QStringLiteral("/bad.dic"), good);
  write_bytes(directory + QStringLiteral("/bad.jdx"),
              std::string(8, '\0'));
  write_bytes(directory + QStringLiteral("/good.dic"), good);

  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"bad.dic", true), entry(u"good.dic", false)};
  const auto loaded = jwpqt::qt::load_edict_resources(registry, directory);
  require(loaded.resources.size() == 1 &&
              loaded.resources[0].registry_index == 1 &&
              loaded.failures.size() == 1 &&
              loaded.failures[0].registry_index == 0 &&
              !loaded.failures[0].quiet,
          "Bad index prevented a later dictionary from loading");

  jwpqt::qt::EdictResourceLoadOptions small;
  small.dictionary_limits.encoded_bytes = 4;
  const auto bounded =
      jwpqt::qt::load_edict_resources(registry, directory, small);
  require(bounded.resources.empty() && bounded.failures.size() == 2,
          "Dictionary byte limit was not applied to each resource");

  jwpqt::qt::EdictResourceLoadOptions aggregate;
  aggregate.resource_entries = 1;
  const auto resource_bounded =
      jwpqt::qt::load_edict_resources(registry, directory, aggregate);
  require(resource_bounded.resources.empty() &&
              resource_bounded.failures.size() == 1 &&
              resource_bounded.truncated,
          "Aggregate resource limit did not include failed attempts");

  aggregate.resource_entries = 2;
  aggregate.dictionary_bytes = good.size();
  const auto byte_bounded =
      jwpqt::qt::load_edict_resources(registry, directory, aggregate);
  require(byte_bounded.resources.empty() && byte_bounded.failures.size() == 2,
          "Aggregate dictionary byte limit did not compose across entries");

  aggregate.dictionary_bytes = 1024;
  aggregate.dictionary_records = 1;
  const auto record_bounded =
      jwpqt::qt::load_edict_resources(registry, directory, aggregate);
  require(record_bounded.resources.empty() &&
              record_bounded.failures.size() == 2,
          "Aggregate dictionary record limit did not compose across entries");

  aggregate.dictionary_records = 10;
  aggregate.definitions = 1;
  const auto definition_bounded =
      jwpqt::qt::load_edict_resources(registry, directory, aggregate);
  require(definition_bounded.resources.empty() &&
              definition_bounded.failures.size() == 2,
          "Aggregate definition limit did not compose across entries");

  aggregate.definitions = 10;
  aggregate.decoded_code_points = 9;
  const auto text_bounded =
      jwpqt::qt::load_edict_resources(registry, directory, aggregate);
  require(text_bounded.resources.empty() && text_bounded.failures.size() == 2,
          "Aggregate decoded-text limit did not compose across entries");
}

void test_mixed_and_ansi_paths(const QString& directory) {
  const QString file_name = QStringLiteral("caf\u00e9.mix");
  const std::string mixed = std::string("word /") +
                            static_cast<char>(0xe9) + "/\n";
  write_bytes(directory + QLatin1Char('/') + file_name, mixed);

  jwpqt::core::EdictRegistry registry;
  registry.wire_encoding =
      jwpqt::core::EdictRegistryWireEncoding::kAnsiBytes;
  jwpqt::core::EdictRegistryEntry mixed_entry =
      entry(std::u16string{u'c', u'a', u'f', static_cast<char16_t>(0xe9),
                           u'.', u'm', u'i', u'x'},
            false);
  mixed_entry.label =
      std::u16string{u'C', u'a', u'f', static_cast<char16_t>(0xe9)};
  mixed_entry.encoding = jwpqt::core::EdictRegistryEncoding::kMixed;
  registry.entries = {mixed_entry};

  const auto loaded = jwpqt::qt::load_edict_resources(registry, directory);
  require(loaded.resources.size() == 1 && loaded.failures.empty() &&
              loaded.resources[0].label == QStringLiteral("Caf\u00e9") &&
              loaded.resources[0].dictionary.records()[0].definitions[0] ==
                  U"\u00e9",
          "ANSI registry path or mixed dictionary page decoded incorrectly");

  jwpqt::qt::EdictResourceLoadOptions fallback;
  fallback.mixed_code_page =
      static_cast<jwpqt::core::LegacyCodePage>(9999);
  const auto fallback_loaded =
      jwpqt::qt::load_edict_resources(registry, directory, fallback);
  require(fallback_loaded.resources.size() == 1 &&
              fallback_loaded.resources[0]
                      .dictionary.records()[0]
                      .definitions[0] == U"\u00e9",
          "Invalid mixed code page did not use the recovered CP1252 fallback");

  registry.entries[0].path[0] = static_cast<char16_t>(0x81);
  const auto invalid = jwpqt::qt::load_edict_resources(registry, directory);
  require(invalid.resources.empty() && invalid.failures.size() == 1,
          "Undefined ANSI path byte did not become an isolated failure");

  registry.entries[0] = mixed_entry;
  registry.entries[0].label[0] = static_cast<char16_t>(0x81);
  const auto invalid_label =
      jwpqt::qt::load_edict_resources(registry, directory);
  require(invalid_label.resources.empty() &&
              invalid_label.failures.size() == 1 &&
              invalid_label.failures[0].source_path ==
                  directory + QLatin1Char('/') + file_name,
          "Invalid ANSI label lost the decoded resource path diagnostic");
}

void test_registry_validation(const QString& directory) {
  jwpqt::core::EdictRegistry invalid;
  invalid.wire_encoding =
      static_cast<jwpqt::core::EdictRegistryWireEncoding>(99);
  bool threw = false;
  try {
    static_cast<void>(jwpqt::qt::load_edict_resources(invalid, directory));
  } catch (const jwpqt::core::EdictRegistryError&) {
    threw = true;
  }
  require(threw, "Invalid registry model was not rejected before loading");

  jwpqt::core::EdictRegistry relative;
  relative.entries = {entry(u"relative.dic", false)};
  const auto failed = jwpqt::qt::load_edict_resources(relative, QString());
  require(failed.resources.empty() && failed.failures.size() == 1,
          "Relative path without a config directory did not fail safely");

  jwpqt::qt::EdictResourceLoadOptions invalid_options;
  invalid_options.ansi_code_page =
      static_cast<jwpqt::core::LegacyCodePage>(9999);
  bool invalid_page_threw = false;
  try {
    static_cast<void>(
        jwpqt::qt::load_edict_resources(relative, directory, invalid_options));
  } catch (const std::runtime_error&) {
    invalid_page_threw = true;
  }
  require(invalid_page_threw,
          "Invalid ANSI code page was not rejected before loading");

  invalid_options.ansi_code_page = jwpqt::core::LegacyCodePage::k1252;
  invalid_options.index_options.utf8_code_page =
      static_cast<jwpqt::core::LegacyCodePage>(9999);
  bool invalid_index_page_threw = false;
  try {
    static_cast<void>(
        jwpqt::qt::load_edict_resources(relative, directory, invalid_options));
  } catch (const std::runtime_error&) {
    invalid_index_page_threw = true;
  }
  require(invalid_index_page_threw,
          "Invalid index code page was not rejected before loading");

  invalid_options.index_options.utf8_code_page =
      jwpqt::core::LegacyCodePage::k1252;
  invalid_options.resource_entries = 0;
  bool invalid_limit_threw = false;
  try {
    static_cast<void>(
        jwpqt::qt::load_edict_resources(relative, directory, invalid_options));
  } catch (const std::runtime_error&) {
    invalid_limit_threw = true;
  }
  require(invalid_limit_threw,
          "Invalid aggregate limit was not rejected before loading");
}

void test_special_files_and_symlinks(const QString& directory) {
  const std::string source = "safe /entry/\n";
  const QString source_path = directory + QStringLiteral("/regular.dic");
  const QString link_path = directory + QStringLiteral("/linked.dic");
  const QString fifo_path = directory + QStringLiteral("/blocked.dic");
  write_bytes(source_path, source);
  require(QFile::link(source_path, link_path),
          "Could not create dictionary symlink fixture");
  const QByteArray fifo_name = QFile::encodeName(fifo_path);
  require(::mkfifo(fifo_name.constData(), 0600) == 0,
          "Could not create dictionary FIFO fixture");

  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"linked.dic", false),
                      entry(u"blocked.dic", false)};
  const auto loaded = jwpqt::qt::load_edict_resources(registry, directory);
  require(loaded.resources.size() == 1 &&
              loaded.resources[0].registry_index == 0 &&
              loaded.failures.size() == 1 &&
              loaded.failures[0].registry_index == 1,
          "Regular symlink or nonblocking FIFO validation is wrong");
}

void test_failed_parse_budget_accounting(const QString& directory) {
  const std::string malformed = "bad /one//two/\n";
  const std::string valid = "good /entry/\n";
  write_bytes(directory + QStringLiteral("/malformed.dic"), malformed);
  write_bytes(directory + QStringLiteral("/after-malformed.dic"), valid);

  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"malformed.dic", false),
                      entry(u"after-malformed.dic", false)};
  jwpqt::qt::EdictResourceLoadOptions options;
  options.definitions = 1;
  const auto definitions =
      jwpqt::qt::load_edict_resources(registry, directory, options);
  require(definitions.resources.empty() && definitions.failures.size() == 2,
          "Malformed dictionary bypassed aggregate definition accounting");

  options.definitions = 10;
  options.decoded_code_points = malformed.size();
  const auto text =
      jwpqt::qt::load_edict_resources(registry, directory, options);
  require(text.resources.empty() && text.failures.size() == 2,
          "Malformed dictionary bypassed aggregate decoded-text accounting");

  write_bytes(directory + QStringLiteral("/malformed.jdx"),
              std::string(8, '\0'));
  write_bytes(directory + QStringLiteral("/after-malformed.jdx"),
              single_entry_index(valid));
  write_bytes(directory + QStringLiteral("/malformed.dic"), valid);
  registry.entries[0].indexed = true;
  registry.entries[1].indexed = true;
  options.decoded_code_points = 1024;
  options.index_entries = 1;
  const auto indexes =
      jwpqt::qt::load_edict_resources(registry, directory, options);
  require(indexes.resources.empty() && indexes.failures.size() == 2,
          "Malformed index bypassed aggregate entry accounting");
}

void test_classical_record_recovery(const QString& directory) {
  const std::string source = "bad [read} /meaning/\ngood /entry/\n";
  write_bytes(directory + QStringLiteral("/classical.dic"), source);
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"classical.dic", false)};
  registry.entries[0].special = jwpqt::core::EdictRegistrySpecial::kClassical;
  const auto recovered = jwpqt::qt::load_edict_resources(registry, directory);
  require(recovered.resources.size() == 1 && recovered.failures.empty() &&
              recovered.resources[0].dictionary.records().size() == 1 &&
              recovered.resources[0].dictionary.record_errors().size() == 1 &&
              recovered.resources[0].dictionary.source_bytes() == source,
          "Classical dictionary recovery dropped valid entries or diagnostics");
  registry.entries[0].indexed = true;
  require(jwpqt::qt::load_edict_resources(registry, directory).resources.empty(),
          "Indexed dictionaries silently skipped malformed source records");
  registry.entries[0].indexed = false;
  registry.entries.push_back(entry(u"classical.dic", false));
  registry.entries[1].special = jwpqt::core::EdictRegistrySpecial::kClassical;
  jwpqt::qt::EdictResourceLoadOptions options;
  options.dictionary_records = 2;
  const auto limited = jwpqt::qt::load_edict_resources(registry, directory, options);
  require(limited.resources.size() == 1 && limited.failures.size() == 1,
          "Skipped classical records bypassed aggregate record accounting");
  write_bytes(directory + QStringLiteral("/classical.dic"), "bad [read} /meaning/\n");
  require(jwpqt::qt::load_edict_resources(registry, directory).resources.empty(),
          "Entirely invalid classical dictionaries appeared available");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  QTemporaryDir temporary_directory(QStringLiteral("/srv/tmp/jwpqt-test-XXXXXX"));
  require(temporary_directory.isValid(), "Could not create temporary directory");

  test_index_path_derivation();
  test_ordered_resource_loading(temporary_directory.path());
  test_failure_isolation_and_limits(temporary_directory.path());
  test_mixed_and_ansi_paths(temporary_directory.path());
  test_registry_validation(temporary_directory.path());
  test_special_files_and_symlinks(temporary_directory.path());
  test_failed_parse_budget_accounting(temporary_directory.path());
  test_classical_record_recovery(temporary_directory.path());

  std::cout << "All EDICT resource tests passed\n";
  return EXIT_SUCCESS;
}
