// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_PROJECT_WORKSPACE_H
#define JWPQT_QT_PROJECT_WORKSPACE_H

#include <optional>
#include <vector>

#include <QString>

#include "application_settings.h"
#include "jwpqt/core/jwp_project.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/text_file.h"

namespace jwpqt::qt {

inline constexpr std::size_t kMaximumWorkspaceDocuments = 1024;

struct ProjectDocument {
  QString path;
  std::optional<core::TextEncoding> encoding;
  core::LegacyCodePage code_page = core::kDefaultLegacyCodePage;
  bool japanese_editing = true;
};

struct ProjectWorkspace {
  ApplicationSettings settings;
  std::vector<ProjectDocument> documents;
  std::size_t current_document = 0;
  // Legacy projects have no per-file formats; null encoding is JWP only when false.
  bool detect_formats = true;
};

struct ProjectPathMapping {
  QString source_directory;
  QString local_directory;
};

class ProjectPathError : public core::JwpProjectError {
 public:
  explicit ProjectPathError(QString source_directory);
  const QString& source_directory() const noexcept { return source_directory_; }

 private:
  QString source_directory_;
};

ProjectWorkspace decode_project_workspace(
    const core::JwpProject& project, const QString& project_path,
    const ApplicationSettings& base = {},
    const std::vector<ProjectPathMapping>& mappings = {}, bool include_settings = true);
core::JwpProject encode_project_workspace(const ProjectWorkspace& workspace, bool include_settings = true);

}  // namespace jwpqt::qt

#endif
