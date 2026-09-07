// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

#include <optional>
#include <string>

#include "jwpqt/core/query_history_file.h"

namespace jwpqt::qt {

struct QueryHistorySnapshot {
  core::QueryHistories histories;
  std::optional<std::string> source;
};

// Absence is optional; every present file must be a valid native archive.
QueryHistorySnapshot read_query_histories(const QString& path);

// The returned bytes become the next expected source. Cooperative writers lock
// the target while checking; external tools that ignore the lock can still race.
std::string write_query_histories(
    const QString& path, const core::QueryHistories& histories,
    const std::optional<std::string>& expected_source);

// Legacy import is read-only and requires the original capacity and code page.
core::QueryHistories import_legacy_query_histories(
    const QString& path, std::size_t storage_cells, core::LegacyCodePage code_page);

}  // namespace jwpqt::qt
