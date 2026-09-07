// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_FILE_IO_H
#define JWPQT_QT_FILE_IO_H

#include <cstddef>
#include <optional>
#include <string>

#include <QString>

#include "jwpqt/core/edict_registry.h"
#include "jwpqt/core/edict_user_dictionary.h"
#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/jwp_project.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/kanji_info.h"
#include "jwpqt/core/kanji_lookup_lists.h"
#include "jwpqt/core/text_file.h"
#include "jwpqt/core/wnn_preferences.h"
#include "jwpqt/core/wnn_user_dictionary.h"

namespace jwpqt::qt {

std::string read_file_bytes(const QString& path);
std::string read_file_bytes(const QString& path, std::size_t maximum_bytes);
core::TextFile read_text_file(const QString& path,
                              core::TextEncoding encoding);
void write_text_file(const QString& path, const core::TextFile& file);
core::JwpDocument read_jwp_file(const QString& path);
void write_jwp_file(const QString& path, const core::JwpDocument& document);
core::JwpProject read_jwp_project_file(const QString& path);
void write_jwp_project_file(const QString& path,
                            const core::JwpProject& project);
std::optional<core::KanjiColorList> read_kanji_color_list_file(
    const QString& path);
void write_kanji_color_list_file(const QString& path,
                                 const core::KanjiColorList& color_list);
std::optional<core::KanjiInfoDatabase> read_kanji_info_file(
    const QString& path,
    const core::KanjiInfoLimits& limits = core::KanjiInfoLimits{});
std::optional<core::KanjiLookupLists> read_kanji_lookup_lists_file(
    const QString& path, std::size_t group_count,
    const core::KanjiLookupListLimits& limits =
        core::KanjiLookupListLimits{});
std::optional<core::WnnPreferences> read_wnn_preferences_file(
    const QString& path,
    std::size_t capacity = core::kWnnDefaultPreferenceCapacity);
void write_wnn_preferences_file(const QString& path,
                                 core::WnnPreferences& preferences);
std::optional<core::WnnUserDictionary> read_wnn_user_dictionary_file(
    const QString& path);
void write_wnn_user_dictionary_file(
    const QString& path, const core::WnnUserDictionary& dictionary);
std::optional<core::EdictRegistry> read_edict_registry_file(
    const QString& path,
    const core::EdictRegistryLimits& limits = core::EdictRegistryLimits{});
void write_edict_registry_file(
    const QString& path, const core::EdictRegistry& registry,
    const core::EdictRegistryLimits& limits = core::EdictRegistryLimits{});
std::optional<core::EdictUserDictionary> read_edict_user_dictionary_file(
    const QString& path,
    core::LegacyCodePage code_page = core::kDefaultLegacyCodePage,
    const core::EdictUserDictionaryLimits& limits =
        core::EdictUserDictionaryLimits{});
void write_edict_user_dictionary_file(
    const QString& path, const core::EdictUserDictionary& dictionary,
    core::LegacyCodePage code_page = core::kDefaultLegacyCodePage,
    const core::EdictUserDictionaryLimits& limits =
        core::EdictUserDictionaryLimits{});

}  // namespace jwpqt::qt

#endif
