// SPDX-License-Identifier: GPL-2.0-or-later

#include "edict_resource_search.h"

#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace jwpqt::qt {
namespace {

void validate_search_options(const EdictResourceSearchOptions& options) {
  if ((options.search.name_filter.category_exclusions & ~core::kEdictCategoryMask) != 0)
    throw core::EdictSearchError("Unknown EDICT category exclusion bits");
  if (options.search.queries == 0 ||
      options.search.candidate_matches == 0 || options.search.results == 0 ||
      options.search.lookup_steps == 0) {
    throw core::EdictSearchError(
        "EDICT resource search limits must be positive");
  }
  (void)core::serialize_edict_registry(core::EdictRegistry{},
                                       options.reload.registry_limits);
}

const EdictLoadedResource* find_loaded(const EdictResourceSet& resources,
                                       std::size_t registry_index) {
  for (const EdictLoadedResource& loaded : resources.resources) {
    if (loaded.registry_index == registry_index) {
      return &loaded;
    }
  }
  return nullptr;
}

EdictResourceFailure remap_failure(const EdictResourceFailure& failure,
                                    std::size_t registry_index) {
  EdictResourceFailure mapped = failure;
  mapped.registry_index = registry_index;
  return mapped;
}

struct AcquiredResource {
  const EdictLoadedResource* borrowed = nullptr;
  std::optional<EdictLoadedResource> owned;

  const EdictLoadedResource* get() const noexcept {
    return owned.has_value() ? &*owned : borrowed;
  }
};

AcquiredResource acquire_resource(const EdictResourceSet& resources,
                                  std::size_t registry_index,
                                  const QString& config_directory,
                                  const EdictResourceLoadOptions& options,
                                  std::vector<EdictResourceFailure>& failures) {
  const core::EdictRegistryEntry& entry =
      resources.registry.entries.at(registry_index);
  const EdictLoadedResource* loaded = find_loaded(resources, registry_index);
  if (entry.keep && loaded != nullptr) {
    return {loaded, std::nullopt};
  }

  core::EdictRegistry one;
  one.wire_encoding = resources.registry.wire_encoding;
  one.entries.push_back(entry);
  EdictResourceSet refreshed =
      load_edict_resources(one, config_directory, options);
  for (const EdictResourceFailure& failure : refreshed.failures) {
    failures.push_back(remap_failure(failure, registry_index));
  }
  if (refreshed.resources.empty()) {
    return {};
  }
  AcquiredResource acquired;
  acquired.owned.emplace(std::move(refreshed.resources.front()));
  acquired.owned->registry_index = registry_index;
  return acquired;
}

core::EdictSearchOptions remaining_options(
    const EdictResourceSearchOptions& options,
    const EdictResourceSearchReport& report) {
  core::EdictSearchOptions remaining = options.search;
  remaining.queries -= report.queries;
  remaining.candidate_matches -= report.candidate_matches;
  remaining.results -= report.results.size();
  remaining.lookup_steps -= report.lookup_steps;
  return remaining;
}

void checked_add(std::size_t& destination, std::size_t value,
                 const char* message) {
  if (destination > std::numeric_limits<std::size_t>::max() - value) {
    throw core::EdictSearchError(message);
  }
  destination += value;
}

core::EdictSearchReport search_one(
    const EdictLoadedResource& resource, const core::EdictSearchPlan& plan,
    const core::EdictSearchOptions& options) {
  if (plan.kind == core::EdictSearchPlanKind::kPattern) {
    return resource.index.has_value()
               ? core::search_edict_pattern(resource.dictionary,
                                             *resource.index, plan, options)
               : core::search_edict_pattern_linear(resource.dictionary, plan,
                                                    options);
  }
  return resource.index.has_value()
             ? core::search_edict(resource.dictionary, *resource.index,
                                  plan.anchor, options)
             : core::search_edict_linear(resource.dictionary, plan.anchor,
                                         options);
}

void merge_report(EdictResourceSearchReport& destination,
                  core::EdictSearchReport source,
                  const EdictLoadedResource& resource) {
  checked_add(destination.rejected, source.rejected,
              "EDICT rejected-result count overflows");
  checked_add(destination.candidate_matches, source.candidate_matches,
              "EDICT candidate count overflows");
  checked_add(destination.queries, source.queries,
              "EDICT query count overflows");
  checked_add(destination.lookup_steps, source.lookup_steps,
              "EDICT lookup work overflows");
  for (auto section : source.sections) {
    section.begin += destination.results.size();
    destination.sections.push_back(section);
  }
  for (core::EdictSearchResult& result : source.results) {
    destination.results.push_back(
        {resource.registry_index, resource.label, std::move(result), resource.entry.special != core::EdictRegistrySpecial::kNormal});
  }
}

void merge_multi_report(
    EdictResourceSearchReport& destination, core::EdictSearchReport source,
    const std::vector<const EdictLoadedResource*>& resources) {
  checked_add(destination.rejected, source.rejected,
              "EDICT rejected-result count overflows");
  checked_add(destination.candidate_matches, source.candidate_matches,
              "EDICT candidate count overflows");
  checked_add(destination.queries, source.queries,
              "EDICT query count overflows");
  checked_add(destination.lookup_steps, source.lookup_steps,
              "EDICT lookup work overflows");
  for (auto section : source.sections) {
    section.begin += destination.results.size();
    destination.sections.push_back(section);
  }
  for (core::EdictSearchResult& result : source.results) {
    if (result.source_index >= resources.size()) {
      throw core::EdictSearchError("EDICT result source is invalid");
    }
    const EdictLoadedResource& resource = *resources[result.source_index];
    destination.results.push_back(
        {resource.registry_index, resource.label, std::move(result), resource.entry.special != core::EdictRegistrySpecial::kNormal});
  }
}

void run_loaded_resources(
    const std::vector<const EdictLoadedResource*>& resources,
    const core::EdictSearchPlan& plan,
    const EdictResourceSearchOptions& options,
    EdictResourceSearchReport& report) {
  if (resources.empty()) {
    return;
  }
  std::vector<core::EdictSearchSource> sources;
  sources.reserve(resources.size());
  for (const EdictLoadedResource* resource : resources) {
    sources.push_back(
        {&resource->dictionary,
         resource->index.has_value() ? &*resource->index : nullptr});
  }
  const core::EdictSearchOptions search = remaining_options(options, report);
  core::EdictSearchReport found =
      plan.kind == core::EdictSearchPlanKind::kPattern
          ? core::search_edict_pattern_sources(sources, plan, search)
          : core::search_edict_sources(sources, plan.anchor, search);
  merge_multi_report(report, std::move(found), resources);
}

void run_loaded_resource(
    const EdictLoadedResource& resource, const core::EdictSearchPlan& plan,
    const EdictResourceSearchOptions& options,
    const core::EdictNameFilterOptions& filter,
    EdictResourceSearchReport& report) {
  core::EdictSearchOptions search = remaining_options(options, report);
  search.name_filter = filter;
  search.adaptive = false;
  search.contingent.names_mode = true;
  merge_report(report, search_one(resource, plan, search), resource);
}

}  // namespace

EdictResourceSearchReport search_edict_resources(
    const EdictResourceSet& resources, const QString& config_directory,
    const core::JwpText& input, const EdictResourceSearchOptions& options) {
  validate_search_options(options);
  (void)core::serialize_edict_registry(resources.registry,
                                       options.reload.registry_limits);
  const core::EdictSearchPlan plan =
      core::prepare_edict_search_plan(input, options.pattern);
  EdictResourceSearchOptions effective = options;
  if (plan.force_open_end) {
    effective.search.direct.require_end = false;
  }
  if (plan.force_closed_boundaries) {
    effective.search.direct.require_beginning = true;
    effective.search.direct.require_end = true;
  }
  if (plan.adaptive_disabled) {
    effective.search.adaptive = false;
    effective.search.contingent.enabled = false;
    effective.search.contingent.forced = false;
  }

  EdictResourceSearchReport report;
  std::vector<AcquiredResource> acquired_resources;
  acquired_resources.reserve(resources.registry.entries.size());
  for (std::size_t index = 0; index < resources.registry.entries.size();
       ++index) {
    const core::EdictRegistryEntry& entry = resources.registry.entries[index];
    if (!entry.searched ||
        entry.names == core::EdictRegistryNames::kNamesOnly ||
        (entry.special == core::EdictRegistrySpecial::kClassical &&
         !effective.classical)) {
      continue;
    }
    AcquiredResource acquired = acquire_resource(
        resources, index, config_directory, effective.reload, report.failures);
    if (acquired.get() != nullptr) {
      acquired_resources.push_back(std::move(acquired));
    }
  }
  std::vector<const EdictLoadedResource*> normal_resources;
  normal_resources.reserve(acquired_resources.size());
  for (const AcquiredResource& acquired : acquired_resources) {
    normal_resources.push_back(acquired.get());
  }
  run_loaded_resources(normal_resources, plan, effective, report);

  if (!effective.personal_names && !effective.place_names) {
    return report;
  }
  for (std::size_t index = 0; index < resources.registry.entries.size();
       ++index) {
    const core::EdictRegistryEntry& entry = resources.registry.entries[index];
    if (!entry.searched || entry.names == core::EdictRegistryNames::kNone ||
        (entry.special == core::EdictRegistrySpecial::kClassical &&
         !effective.classical)) {
      continue;
    }
    AcquiredResource acquired = acquire_resource(
        resources, index, config_directory, effective.reload, report.failures);
    const EdictLoadedResource* resource = acquired.get();
    if (resource == nullptr) {
      continue;
    }
    if (effective.personal_names) {
      core::EdictNameFilterOptions filter;
      filter.category_exclusions = effective.search.name_filter.category_exclusions;
      filter.reject_place_names = true;
      run_loaded_resource(*resource, plan, effective, filter, report);
    }
    if (effective.place_names) {
      core::EdictNameFilterOptions filter;
      filter.category_exclusions = effective.search.name_filter.category_exclusions;
      filter.reject_personal_names = true;
      run_loaded_resource(*resource, plan, effective, filter, report);
    }
    break;
  }
  return report;
}

}  // namespace jwpqt::qt
