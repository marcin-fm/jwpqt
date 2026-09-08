// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <iostream>
#include <stdexcept>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "project_workspace.h"
#include "text_bridge.h"

namespace {
using namespace jwpqt;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void rejects(const std::function<void()>& action) {
  try { action(); } catch (const std::exception&) { return; }
  throw std::runtime_error("Invalid project workspace was accepted");
}

void roundtrip(const QString& directory) {
  qt::ProjectWorkspace workspace;
  workspace.detect_formats = false;
  workspace.settings = qt::read_application_settings("Future_Field = raw\r\nFile.Size = 23\r\nDict_Link_Adv_NoNames=true\n"
      "Dict_Compact=true\nDict_AutoSearch=false\nDict_AdvancedSearches=true\nDict_ContingentSearches=true\nDict_ExclusionFilters=0x10002\nHistoryBuffers_NumChars=512\nSave_Histories=false\n");
  workspace.documents = {{directory + "/first.jwp", {}, core::LegacyCodePage::k1251, true},
      {directory + "/second.txt", core::TextEncoding::kUtf16Be, core::LegacyCodePage::k1252, false},
      {directory + "/third\\name.jfc", core::TextEncoding::kJfc, core::LegacyCodePage::k1258, true}};
  for (std::size_t active = 0; active < workspace.documents.size(); ++active) {
    workspace.current_document = active;
    const auto encoded = qt::encode_project_workspace(workspace);
    require(encoded.paths.back() == qt::from_qstring(workspace.documents[active].path), "JPR active path is not last");
    const auto wire = core::serialize_jwp_project(encoded);
    const auto decoded = qt::decode_project_workspace(core::parse_jwp_project(wire), directory + "/state.jpr");
    require(!decoded.detect_formats && decoded.current_document == active && decoded.documents.size() == 3,
            "Native project lost its tab order or active file");
    require(decoded.settings.fonts[static_cast<int>(qt::JapaneseFontRole::kFile)].size == 23 &&
            decoded.settings.unapplied.contains("Future_Field"), "Project settings were not retained/applied");
    require(decoded.settings.dictionary.link_advanced_names && decoded.settings.dictionary.compact && !decoded.settings.dictionary.automatic_search && decoded.settings.dictionary.contingent && decoded.settings.dictionary.advanced && decoded.settings.dictionary.require_end &&
            !decoded.settings.dictionary.require_beginning && decoded.settings.dictionary.personal_names &&
            decoded.settings.dictionary.place_names && decoded.settings.dictionary.category_exclusions == 0x10000U &&
            decoded.settings.dictionary_extra_exclusions == 0,
            "Project lost dictionary policies or retained exclusion bits");
    require(decoded.settings.history_size == 512 && !decoded.settings.save_histories,
            "Project lost history capacity or persistence policy");
    for (std::size_t i = 0; i < decoded.documents.size(); ++i) {
      const auto& left = decoded.documents[i];
      const auto& right = workspace.documents[i];
      require(left.path == right.path && left.encoding == right.encoding && left.code_page == right.code_page &&
              left.japanese_editing == right.japanese_editing, "Project format or Unicode editing mode changed");
    }
    require(core::serialize_jwp_project(qt::encode_project_workspace(decoded)) == wire,
            "Project metadata grew or changed on repeated saves");
  }
  for (unsigned i = 4; i <= 24; ++i) {
    workspace.settings.dictionary.category_exclusions = std::uint32_t{1} << i;
    workspace.settings.dictionary_extra_exclusions = 0x80000000U;
    const auto wire = core::serialize_jwp_project(qt::encode_project_workspace(workspace));
    const auto restored = qt::decode_project_workspace(core::parse_jwp_project(wire), directory + "/category.jpr");
    require(restored.settings.dictionary.category_exclusions == (std::uint32_t{1} << i) &&
                restored.settings.dictionary_extra_exclusions == 0x80000000U &&
                core::serialize_jwp_project(qt::encode_project_workspace(restored)) == wire,
            "JPR lost an independent category filter or its retained unknown bits");
  }
  workspace.documents.clear(); workspace.current_document = 0;
  const auto empty = qt::decode_project_workspace(qt::encode_project_workspace(workspace), directory + "/empty.jpr");
  require(empty.documents.empty() && !empty.detect_formats && empty.current_document == 0, "Empty workspace failed");
  for (const auto encoding : {core::TextEncoding::kUtf8, core::TextEncoding::kUtf7,
      core::TextEncoding::kUtf16Le, core::TextEncoding::kUtf16Be, core::TextEncoding::kJfc,
      core::TextEncoding::kEucJp, core::TextEncoding::kShiftJis, core::TextEncoding::kNewJis,
      core::TextEncoding::kOldJis, core::TextEncoding::kNecJis}) {
    workspace.documents = {{directory + "/all.txt", encoding}};
    require(qt::decode_project_workspace(qt::encode_project_workspace(workspace), directory + "/all.jpr")
                .documents[0].encoding == encoding, "A native project text encoding was lost");
  }
}

void legacy_paths(const QString& directory) {
  core::JwpProject legacy{"TranslationCodePage = 1251\r\n", U"C:\\Work", {U"sub\\first.jwp", U"C:\\Work\\two.txt"}};
  bool requested = false;
  try { (void)qt::decode_project_workspace(legacy, directory + "/legacy.jpr"); }
  catch (const qt::ProjectPathError& error) { requested = error.source_directory() == "C:/Work"; }
  require(requested, "Windows directory was guessed instead of requested");
  const std::vector<qt::ProjectPathMapping> mappings{{"c:\\work", directory}};
  const auto decoded = qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, mappings);
  require(decoded.detect_formats && decoded.current_document == 1 && !decoded.documents[0].encoding &&
          decoded.documents[0].code_page == core::LegacyCodePage::k1251 &&
          decoded.documents[0].path == directory + "/sub/first.jwp", "Legacy project mapping/default format failed");
  legacy.paths = {U"D:\\Elsewhere\\third.jwp"};
  rejects([&] { qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, mappings); });
  auto two_roots = mappings; two_roots.push_back({"D:/Elsewhere", directory});
  require(qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, two_roots).documents[0].path ==
          directory + "/third.jwp", "A second Windows volume cannot be mapped");
  legacy.current_directory = U"\\\\host\\share\\work";
  legacy.paths = {U"a.jwp"};
  require(qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, {{"//HOST/share/work", directory}})
              .documents[0].path == directory + "/a.jwp", "UNC project mapping failed");
  legacy.current_directory = U"C:\\Work"; legacy.paths = {U"C:relative.jwp"};
  rejects([&] { qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, mappings); });
  legacy.paths = {U"C:\\Work2\\a.jwp"};
  rejects([&] { qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, mappings); });
  legacy.paths = {U"C:\\Work\\a.jwp"};
  auto duplicate = mappings; duplicate.push_back({"C:/WORK", directory});
  rejects([&] { qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, duplicate); });
  rejects([&] { qt::decode_project_workspace(legacy, directory + "/legacy.jpr", {}, {{"C:/Work", "relative"}}); });
  legacy.current_directory = U"sub"; legacy.paths = {U"file.txt"};
  require(qt::decode_project_workspace(legacy, directory + "/legacy.jpr").documents[0].path ==
          directory + "/sub/file.txt", "Relative stored directory did not use project location");
}

void invalid_metadata(const QString& directory) {
  const auto test = [&](const std::string& json) {
    core::JwpProject project{"# jwpqt-project " + json + "\n", {}, {U"a.txt"}};
    rejects([&] { qt::decode_project_workspace(project, directory + "/bad.jpr"); });
  };
  test("{}"); test("[]"); test("null"); test("not json");
  const std::string valid = R"({"version":1,"paths":"posix","first":0,"formats":[{"encoding":"utf-16be","code_page":1252,"japanese":false}]})";
  for (const auto& change : {std::pair<std::string, std::string>{"\"version\":1", "\"version\":2"},
      {"\"first\":0", "\"first\":1"}, {"\"first\":0", "\"first\":0.5"},
      {"1252", "1252.5"}, {"1252", "0"}, {"false", "0"}, {"utf-16be", "unknown"},
      {"posix", "windows"}}) {
    auto value = valid; value.replace(value.find(change.first), change.first.size(), change.second); test(value);
  }
  core::JwpProject duplicate{"# jwpqt-project " + valid + "\n# jwpqt-project " + valid, {}, {U"a.txt"}};
  rejects([&] { qt::decode_project_workspace(duplicate, directory + "/bad.jpr"); });
  duplicate.configuration = "# jwpqt-project " + valid; duplicate.paths.push_back(U"b.txt");
  rejects([&] { qt::decode_project_workspace(duplicate, directory + "/bad.jpr"); });
  duplicate.configuration = "File.Size = bad\nFile.Size = 16\n";
  rejects([&] { qt::decode_project_workspace(duplicate, directory + "/bad.jpr"); });
  qt::ProjectWorkspace invalid; invalid.detect_formats = false;
  invalid.documents = {{"relative", core::TextEncoding::kUtf8}};
  rejects([&] { qt::encode_project_workspace(invalid); });
  invalid.documents[0].path = directory + "/a.txt"; invalid.current_document = 1;
  rejects([&] { qt::encode_project_workspace(invalid); });
  invalid.current_document = 0; invalid.documents.push_back(invalid.documents.front());
  rejects([&] { qt::encode_project_workspace(invalid); });
  invalid.documents.resize(1);
  invalid.documents[0].encoding = static_cast<core::TextEncoding>(999);
  rejects([&] { qt::encode_project_workspace(invalid); });
  invalid.documents[0].encoding = core::TextEncoding::kUtf8;
  invalid.documents[0].path += QChar(0);
  rejects([&] { qt::encode_project_workspace(invalid); });
  invalid.documents[0].path = directory + QChar(0xd800);
  rejects([&] { qt::encode_project_workspace(invalid); });
}

void symlinks_and_limits(const QString& directory) {
  require(QDir().mkpath(directory + "/target/deep"), "Cannot create symlink fixture");
  QFile target(directory + "/target/file.txt"); require(target.open(QIODevice::WriteOnly), "Cannot create target"); target.close();
  QFile other(directory + "/file.txt"); require(other.open(QIODevice::WriteOnly), "Cannot create other target"); other.close();
  require(QFile::link(directory + "/target/deep", directory + "/link"), "Cannot create directory symlink");
  qt::ProjectWorkspace workspace; workspace.detect_formats = false;
  workspace.documents = {{directory + "/link/../file.txt", core::TextEncoding::kUtf8},
                          {directory + "/file.txt", core::TextEncoding::kUtf8}};
  const auto decoded = qt::decode_project_workspace(qt::encode_project_workspace(workspace), directory + "/symlink.jpr");
  require(decoded.documents[0].path == workspace.documents[0].path && decoded.documents[1].path == workspace.documents[1].path,
          "Project paths were cleaned across a directory symlink");
  workspace.documents[1].path = directory + "/target/file.txt";
  rejects([&] { qt::encode_project_workspace(workspace); });
  workspace.documents.clear();
  for (std::size_t i = 0; i < qt::kMaximumWorkspaceDocuments; ++i)
    workspace.documents.push_back({directory + "/limit" + QString::number(i), core::TextEncoding::kUtf16Le});
  auto wire = qt::encode_project_workspace(workspace);
  require(qt::decode_project_workspace(wire, directory + "/limit.jpr").documents.size() == qt::kMaximumWorkspaceDocuments,
          "Maximum workspace cannot round trip");
  workspace.documents.push_back({directory + "/overflow.txt", {}});
  rejects([&] { qt::encode_project_workspace(workspace); });
  wire.paths.push_back(U"overflow.txt");
  rejects([&] { qt::decode_project_workspace(wire, directory + "/limit.jpr"); });
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication application(argc, argv);
  try {
    QTemporaryDir directory;
    require(directory.isValid(), "Cannot create project fixtures");
    roundtrip(directory.path()); legacy_paths(directory.path());
    invalid_metadata(directory.path()); symlinks_and_limits(directory.path());
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
