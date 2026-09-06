// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/text_file.h"

namespace jwpqt::qt {

inline constexpr std::size_t kMaximumRecentDocuments = 9;

struct RecentDocument {
  QString path;
  std::optional<core::TextEncoding> encoding;
  core::LegacyCodePage code_page = core::kDefaultLegacyCodePage;
};

class RecentFilesError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

std::vector<RecentDocument> read_recent_documents(const QString& path);
void write_recent_documents(const QString& path,
                            const std::vector<RecentDocument>& documents);

}  // namespace jwpqt::qt
