// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef JWPQT_QT_SESSION_IO_H
#define JWPQT_QT_SESSION_IO_H

#include <optional>
#include <string>

#include "project_workspace.h"

namespace jwpqt::qt {
// Missing is a known empty source; malformed/present sources throw.
std::optional<std::string> read_session_source(const QString& path);
std::string write_session_source(const QString& path, const ProjectWorkspace& workspace,
                                const std::optional<std::string>& expected);
}  // namespace jwpqt::qt
#endif
