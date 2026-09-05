// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <QString>

#include "jwpqt/core/edict_dictionary.h"
#include "jwpqt/core/edict_index.h"
#include "jwpqt/core/edict_registry.h"
#include "jwpqt/core/legacy_code_page.h"

namespace jwpqt::qt {

struct EdictResourceLoadOptions {
  core::EdictRegistryLimits registry_limits;
  core::EdictParseLimits dictionary_limits;
  core::EdictIndexOptions index_options;
  core::LegacyCodePage ansi_code_page = core::kDefaultLegacyCodePage;
  core::LegacyCodePage mixed_code_page = core::kDefaultLegacyCodePage;
  std::size_t resource_entries = 4096;
  std::size_t dictionary_bytes = 256U * 1024U * 1024U;
  std::size_t index_bytes = 128U * 1024U * 1024U;
  std::size_t dictionary_records = 2'000'000;
  std::size_t definitions = 8'000'000;
  std::size_t decoded_code_points = 64U * 1024U * 1024U;
  std::size_t index_entries = 8'000'000;
};

struct EdictLoadedResource {
  std::size_t registry_index = 0;
  core::EdictRegistryEntry entry;
  QString label;
  QString source_path;
  std::optional<QString> index_path;
  core::EdictDictionary dictionary;
  std::optional<core::EdictIndex> index;
};

struct EdictResourceFailure {
  std::size_t registry_index = 0;
  QString source_path;
  QString message;
  bool quiet = false;
};

struct EdictResourceSet {
  core::EdictRegistry registry;
  std::vector<EdictLoadedResource> resources;
  std::vector<EdictResourceFailure> failures;
  bool truncated = false;
};

QString edict_index_path(const QString& source_path);

EdictResourceSet load_edict_resources(
    const core::EdictRegistry& registry, const QString& config_directory,
    const EdictResourceLoadOptions& options = EdictResourceLoadOptions{});

}  // namespace jwpqt::qt
