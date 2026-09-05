// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "edict_resource_search.h"
#include "edict_resources.h"
#include "jwpqt/core/edict_registry.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/utf8.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

template <typename Function>
void require_throws(Function&& function, const char* message) {
  try {
    function();
  } catch (const std::exception&) {
    return;
  }
  require(false, message);
}

void write_file(const QString& path, const std::string& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly), "Could not write search fixture");
  require(file.write(bytes.data(), static_cast<qint64>(bytes.size())) ==
              static_cast<qint64>(bytes.size()),
          "Could not write complete search fixture");
}

jwpqt::core::EdictRegistryEntry entry(std::u16string label,
                                       std::u16string path,
                                       bool names_only = false,
                                       bool keep = true) {
  jwpqt::core::EdictRegistryEntry value;
  value.label = std::move(label);
  value.path = std::move(path);
  value.encoding = jwpqt::core::EdictRegistryEncoding::kUtf8;
  value.names = names_only ? jwpqt::core::EdictRegistryNames::kNamesOnly
                           : jwpqt::core::EdictRegistryNames::kNone;
  value.searched = true;
  value.keep = keep;
  return value;
}

jwpqt::core::JwpText query(std::u32string_view text) {
  return jwpqt::core::encode_jwp_text(text);
}

void test_ordered_linear_resources(const QString& directory) {
  write_file(directory + "/first", "cat /first/\n");
  write_file(directory + "/second", "cat /second/\n");
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"First", u"first"),
                      entry(u"Second", u"second")};
  const jwpqt::qt::EdictResourceSet resources =
      jwpqt::qt::load_edict_resources(registry, directory);
  const jwpqt::qt::EdictResourceSearchReport report =
      jwpqt::qt::search_edict_resources(resources, directory, query(U"cat"));
  require(report.results.size() == 2 &&
              report.results[0].registry_index == 0 &&
              report.results[0].label == QStringLiteral("First") &&
              report.results[1].registry_index == 1 &&
              report.results[1].result.record.definitions ==
                  std::vector<std::u32string>{U"second"},
          "Resource search did not preserve configured result order");
}

void test_names_only_pseudo_passes(const QString& directory) {
  write_file(directory + "/normal", "alice /ordinary/\n");
  write_file(directory + "/names",
             "alice /(s) person/\nalice /(p) place/\n");
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"Normal", u"normal"),
                      entry(u"Names", u"names", true)};
  const jwpqt::qt::EdictResourceSet resources =
      jwpqt::qt::load_edict_resources(registry, directory);
  jwpqt::qt::EdictResourceSearchOptions options;
  options.personal_names = true;
  options.place_names = true;
  const jwpqt::qt::EdictResourceSearchReport report =
      jwpqt::qt::search_edict_resources(resources, directory, query(U"alice"),
                                        options);
  require(report.results.size() == 3 &&
              report.results[0].label == QStringLiteral("Normal") &&
              report.results[1].result.record.definitions ==
                  std::vector<std::u32string>{U"(s) person"} &&
              report.results[2].result.record.definitions ==
                  std::vector<std::u32string>{U"(p) place"},
          "Name pseudo passes did not filter or follow normal resources");
}

void test_non_keep_reload_and_failure(const QString& directory) {
  const QString path = directory + "/changing";
  write_file(path, "cat /old/\n");
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"Changing", u"changing", false, false)};
  const jwpqt::qt::EdictResourceSet resources =
      jwpqt::qt::load_edict_resources(registry, directory);
  write_file(path, "dog /new/\n");
  const jwpqt::qt::EdictResourceSearchReport refreshed =
      jwpqt::qt::search_edict_resources(resources, directory, query(U"dog"));
  require(refreshed.results.size() == 1 &&
              refreshed.results[0].result.record.definitions ==
                  std::vector<std::u32string>{U"new"},
          "Non-keep resource did not refresh before search");

  require(QFile::remove(path), "Could not remove reload failure fixture");
  const jwpqt::qt::EdictResourceSearchReport missing =
      jwpqt::qt::search_edict_resources(resources, directory, query(U"dog"));
  require(missing.results.empty() && missing.failures.size() == 1 &&
              missing.failures[0].registry_index == 0,
          "Reload failure was not retained without stale search results");
}

void test_global_budgets_span_resources(const QString& directory) {
  write_file(directory + "/one", "cat /one/\n");
  write_file(directory + "/two", "cat /two/\n");
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"One", u"one"), entry(u"Two", u"two")};
  const jwpqt::qt::EdictResourceSet resources =
      jwpqt::qt::load_edict_resources(registry, directory);
  jwpqt::qt::EdictResourceSearchOptions options;
  options.search.results = 1;
  require_throws(
      [&] {
        (void)jwpqt::qt::search_edict_resources(resources, directory,
                                                 query(U"cat"), options);
      },
      "Accepted-result budget did not span resources");
  options.search.results = 10;
  options.search.queries = 1;
  require_throws(
      [&] {
        (void)jwpqt::qt::search_edict_resources(resources, directory,
                                                 query(U"cat"), options);
      },
      "Query budget did not span resources");
}

void test_classical_opt_in(const QString& directory) {
  write_file(directory + "/modern", "cat /modern/\n");
  write_file(directory + "/classical", "cat /classical/\n");
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"Modern", u"modern"),
                      entry(u"Classical", u"classical")};
  registry.entries[1].special =
      jwpqt::core::EdictRegistrySpecial::kClassical;
  const jwpqt::qt::EdictResourceSet resources =
      jwpqt::qt::load_edict_resources(registry, directory);
  jwpqt::qt::EdictResourceSearchOptions options;
  const auto modern = jwpqt::qt::search_edict_resources(
      resources, directory, query(U"cat"), options);
  require(modern.results.size() == 1 &&
              modern.results[0].label == QStringLiteral("Modern"),
          "Classical resource was searched without explicit opt-in");
  options.classical = true;
  const auto both = jwpqt::qt::search_edict_resources(
      resources, directory, query(U"cat"), options);
  require(both.results.size() == 2 &&
              both.results[0].label == QStringLiteral("Modern") &&
              both.results[1].label == QStringLiteral("Classical"),
          "Classical opt-in did not preserve registry order");
}

void test_names_fall_through_failed_resource(const QString& directory) {
  write_file(directory + "/later-names", "alice /(s) person/\n");
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"Missing", u"missing-names", true, false),
                      entry(u"Later", u"later-names", true)};
  registry.entries[0].quiet = true;
  const jwpqt::qt::EdictResourceSet resources =
      jwpqt::qt::load_edict_resources(registry, directory);
  jwpqt::qt::EdictResourceSearchOptions options;
  options.personal_names = true;
  const auto report = jwpqt::qt::search_edict_resources(
      resources, directory, query(U"alice"), options);
  require(report.results.size() == 1 &&
              report.results[0].registry_index == 1 &&
              report.results[0].label == QStringLiteral("Later") &&
              report.failures.size() == 1 && report.failures[0].quiet,
          "Failed names resource suppressed a later usable resource");
}

void test_search_plan_flags_span_resources(const QString& directory) {
  const std::string open = jwpqt::core::encode_utf8(U"\u3042\u3044\u3046") +
                           " /open/\n";
  const std::string adaptive =
      jwpqt::core::encode_utf8(U"\u3042\u304f") + " /adaptive/\n";
  write_file(directory + "/plans", open + adaptive);
  jwpqt::core::EdictRegistry registry;
  registry.entries = {entry(u"Plans", u"plans")};
  const jwpqt::qt::EdictResourceSet resources =
      jwpqt::qt::load_edict_resources(registry, directory);

  jwpqt::qt::EdictResourceSearchOptions options;
  options.search.adaptive = true;
  options.search.adaptive_always = false;
  options.search.direct.require_end = true;
  const jwpqt::qt::EdictResourceSearchReport opened =
      jwpqt::qt::search_edict_resources(
          resources, directory,
          jwpqt::core::JwpText{0x2522, 0x2424, '*'}, options);
  require(opened.results.size() == 1 && opened.queries == 1 &&
              opened.results[0].result.record.definitions ==
                  std::vector<std::u32string>{U"open"},
          "Trailing-star plan did not open End and disable adaptive search");
}

}  // namespace

int main() {
  QTemporaryDir temporary(QStringLiteral("/srv/tmp/jwpqt-edict-search-XXXXXX"));
  require(temporary.isValid(), "Could not create resource search directory");
  test_ordered_linear_resources(temporary.path());
  test_names_only_pseudo_passes(temporary.path());
  test_non_keep_reload_and_failure(temporary.path());
  test_global_budgets_span_resources(temporary.path());
  test_classical_opt_in(temporary.path());
  test_names_fall_through_failed_resource(temporary.path());
  test_search_plan_flags_span_resources(temporary.path());
  return EXIT_SUCCESS;
}
