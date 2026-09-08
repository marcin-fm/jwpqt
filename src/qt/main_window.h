// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_MAIN_WINDOW_H
#define JWPQT_QT_MAIN_WINDOW_H

#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QIcon>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QPixmap>
#include <QString>

#include "application_settings.h"
#include "character_context_menu.h"
#include "input_mode.h"
#include "project_workspace.h"
#include "query_history_io.h"
#include "recent_files.h"
#include "jwpqt/core/jwp_conversion.h"
#include "jwpqt/core/jwp_document_history.h"
#include "jwpqt/core/jwp_document_model.h"
#include "jwpqt/core/jwp_search.h"
#include "jwpqt/core/kanji_color.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/kanji_info.h"
#include "jwpqt/core/kanji_lookup_lists.h"
#include "jwpqt/core/kana_input.h"
#include "jwpqt/core/edict_user_dictionary.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/query_history.h"
#include "jwpqt/core/text_file.h"
#include "jwpqt/core/wnn_dictionary.h"
#include "jwpqt/core/wnn_preferences.h"
#include "jwpqt/core/wnn_user_dictionary.h"

class QAction;
class QActionGroup;
class QCloseEvent;
class QEvent;
class QLabel;
class QListWidget;
class QMenu;
class QPrinter;
class QTabWidget;
class QTextCursor;
class QTextEdit;
class QToolBar;
class QToolButton;

namespace jwpqt::qt {

class EdictLookupDialog;
class FindReplaceDialog;
struct FindReplaceRequest;
struct FindReplaceResult;
struct EdictLookupOptions;
class EdictResultsWindow;
class EdictUserDictionaryDialog;
struct EdictResourceSet;
class JisTableDialog;
class JwpEditor;
class KanjiCodeLookupDialog;
class KanjiCountDialog;
class KanjiLookupDialog;
class KanjiReadingLookupDialog;
class WnnUserDictionaryDialog;

enum class OpenMode {
  kInteractive,
  kNonInteractive,
};

enum class KanjiCodeLookupMode {
  kSkip,
  kFourCorner,
  kBushu,
  kSpahn,
  kStrokeBushu,
  kIndex,
};

struct SearchRequest {
  QString text;
  core::JwpSearchOptions options;
};

enum class ReplaceMode {
  kNext,
  kAll,
};

struct ReplaceRequest {
  QString text;
  QString replacement;
  core::JwpSearchOptions options;
  ReplaceMode mode = ReplaceMode::kNext;
};

struct KanjiColorListEditRequest {
  QString text;
  bool add = true;
};

struct ProjectOpenOptions {
  bool append = false;
  bool allow_unapplied_settings = false;
  std::optional<core::TextEncoding> legacy_encoding;
  std::vector<ProjectPathMapping> path_mappings;
};

struct LegacyHistoryOptions {
  std::size_t storage_cells;
  core::LegacyCodePage code_page;
};

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  JwpEditor* active_editor() const noexcept;
  int document_count() const noexcept;
  int current_document_index() const noexcept;
  int new_document_tab(bool japanese_editing = true);
  bool activate_document(int index);
  bool close_document(int index, OpenMode mode = OpenMode::kInteractive);
  bool close_all_documents(OpenMode mode = OpenMode::kInteractive);
  bool save_all_documents(OpenMode mode = OpenMode::kInteractive);
  bool load_recent_file_configuration(
      const QString& path, OpenMode mode = OpenMode::kNonInteractive);
  const std::vector<RecentDocument>& recent_documents() const noexcept;
  QString recent_file_warning() const;
  bool open_recent_document(int index, OpenMode mode = OpenMode::kNonInteractive);
  bool clear_recent_documents(OpenMode mode = OpenMode::kNonInteractive);
  const ApplicationSettings& application_settings() const noexcept;
  QString application_settings_warning() const;
  bool apply_application_settings(const ApplicationSettings& settings,
                                  OpenMode mode = OpenMode::kNonInteractive);
  bool load_application_settings(const QString& path,
                                 OpenMode mode = OpenMode::kNonInteractive);
  bool import_application_settings(const QString& path,
                                   OpenMode mode = OpenMode::kNonInteractive);
  bool save_application_settings(const QString& path = {},
                                 OpenMode mode = OpenMode::kNonInteractive);
  const core::QueryHistories& query_histories() const noexcept;
  QString query_history_warning() const;
  bool load_query_history(const QString& path, OpenMode mode = OpenMode::kNonInteractive);
  bool save_query_history(const QString& path = {}, OpenMode mode = OpenMode::kNonInteractive);
  bool import_query_history(const QString& path,
                            const std::optional<LegacyHistoryOptions>& legacy = {},
                            OpenMode mode = OpenMode::kNonInteractive);
  bool clear_query_history(OpenMode mode = OpenMode::kNonInteractive);
  bool open_project_path(const QString& path, const ProjectOpenOptions& options = {},
                         OpenMode mode = OpenMode::kNonInteractive);
  bool save_project_path(const QString& path, bool save_documents = true,
                         OpenMode mode = OpenMode::kNonInteractive);
  QString current_project_path() const;
  QString project_warning() const;

  // A new-tab open preserves other buffers and activates already-open paths.
  bool open_path(const QString& path, core::TextEncoding encoding,
                 OpenMode mode = OpenMode::kInteractive, bool new_tab = false);
  bool open_jwp_path(
      const QString& path,
      core::LegacyCodePage code_page = core::kDefaultLegacyCodePage,
      OpenMode mode = OpenMode::kInteractive, bool new_tab = false);
  bool open_path_detected(const QString& path,
                          OpenMode mode = OpenMode::kInteractive,
                          bool new_tab = false);
  bool save_path(const QString& path);
  // An absent encoding selects the JWP container; text losses require consent.
  bool save_as_path(const QString& path,
                    std::optional<core::TextEncoding> encoding,
                    bool allow_format_loss = false, bool export_copy = false,
                    OpenMode mode = OpenMode::kNonInteractive);
  bool uses_jwp_format() const noexcept;
  bool set_japanese_editing(bool enabled, bool allow_information_loss = false,
                            OpenMode mode = OpenMode::kNonInteractive);
  bool revert_current_document(OpenMode mode = OpenMode::kInteractive);
  bool delete_current_document(OpenMode mode = OpenMode::kInteractive);
  QString current_path() const;
  bool document_modified() const noexcept;
  bool load_wnn_resources(const QString& index_path,
                          const QString& data_path,
                          const QString& preferences_path,
                          OpenMode mode = OpenMode::kInteractive);
  bool load_wnn_resources(const QString& index_path,
                          const QString& data_path,
                          const QString& preferences_path,
                          const QString& user_dictionary_path,
                          OpenMode mode = OpenMode::kInteractive);
  bool set_wnn_user_dictionary(
      core::WnnUserDictionary user_dictionary,
      OpenMode mode = OpenMode::kInteractive);
  const core::WnnUserDictionary* wnn_user_dictionary() const noexcept;
  bool insert_wnn_user_entry(const core::WnnUserEntry& entry);
  bool convert_selection();
  bool cycle_conversion(bool previous = false);
  bool accept_conversion();
  bool conversion_active() const noexcept;
  bool kana_input_enabled() const noexcept;
  core::TextEncoding text_encoding() const noexcept;
  bool is_jwp_document() const noexcept;
  core::LegacyCodePage jwp_code_page() const noexcept;
  const core::JwpDocument* current_jwp_document() const noexcept;
  bool find_text(const QString& text, core::JwpSearchOptions options = {});
  bool replace_next(const QString& text, const QString& replacement,
                    core::JwpSearchOptions options = {});
  std::size_t replace_all(const QString& text, const QString& replacement,
                          core::JwpSearchOptions options = {});
  bool format_paragraphs(const core::JwpParagraphFormat& format);
  bool insert_page_break();
  bool load_kanji_color_configuration(
      const QString& settings_path, const QString& list_path,
      OpenMode mode = OpenMode::kInteractive);
  bool load_edict_configuration(
      const QString& registry_path,
      OpenMode mode = OpenMode::kInteractive);
  bool load_kanji_info(const QString& path,
                       OpenMode mode = OpenMode::kInteractive);
  bool load_kanji_lookup(const QString& radical_path,
                         const QString& stroke_path,
                         const QString& radical_bitmap_path,
                         OpenMode mode = OpenMode::kInteractive);
  const core::KanjiInfoDatabase* kanji_info_database() const noexcept;
  bool has_kanji_lookup() const noexcept;
  const EdictResourceSet* edict_resources() const noexcept;
  const core::EdictUserDictionary* edict_user_dictionary() const noexcept;
  bool set_edict_user_dictionary(
      core::EdictUserDictionary dictionary,
      OpenMode mode = OpenMode::kInteractive);
  bool insert_edict_user_entry(const core::EdictUserEntry& entry);
  bool insert_edict_text(std::u32string_view text);
  bool set_kanji_color_policy(
      const core::KanjiColorPolicy& policy,
      OpenMode mode = OpenMode::kInteractive);
  bool set_kanji_color_list(
      core::KanjiColorList color_list,
      OpenMode mode = OpenMode::kInteractive);
  bool make_kanji_color_list(OpenMode mode = OpenMode::kInteractive);
  bool append_kanji_color_list(OpenMode mode = OpenMode::kInteractive);
  bool edit_kanji_color_list(
      const QString& text, bool add,
      OpenMode mode = OpenMode::kInteractive);
  bool view_kanji_color_list(OpenMode mode = OpenMode::kInteractive);
  bool clear_kanji_color_list(OpenMode mode = OpenMode::kInteractive);
  const core::KanjiColorPolicy& kanji_color_policy() const noexcept;
  const core::KanjiColorList& kanji_color_list() const noexcept;
  QString resource_report() const;

 protected:
  void changeEvent(QEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  virtual std::optional<core::TextEncoding> prompt_for_encoding(
      const std::vector<core::TextEncoding>& candidates,
      const QString& explanation);
  virtual std::optional<SearchRequest> prompt_for_search(
      const SearchRequest& initial);
  virtual std::optional<ReplaceRequest> prompt_for_replace(
      const ReplaceRequest& initial);
  virtual std::optional<core::JwpParagraphFormat>
  prompt_for_paragraph_format(const core::JwpParagraphFormat& initial);
  virtual std::optional<core::JwpDocument> prompt_for_page_layout(
      const core::JwpDocument& initial);
  virtual bool prompt_for_print(QPrinter& printer);
  virtual bool prompt_for_printer_setup(QPrinter& printer);
  virtual bool prompt_to_revert(const QString& path);
  virtual bool prompt_to_delete(const QString& path);
  virtual std::optional<core::KanjiColorPolicy>
  prompt_for_kanji_color_policy(const core::KanjiColorPolicy& initial);
  virtual std::optional<KanjiColorListEditRequest>
  prompt_for_kanji_color_list_edit();

 private:
  struct DocumentState;
  struct WnnResources;
  struct EdictUserResources;

  void create_actions();
  void update_command_bar_palette();
  void update_resource_status();
  void undo_document();
  void redo_document();
  void restore_jwp_history_state(core::JwpPosition caret);
  void update_undo_actions();
  void update_conversion_actions();
  void update_edict_actions();
  void update_kanji_info_action();
  void update_jis_table_action();
  void update_kanji_count_action();
  void update_kanji_code_lookup_actions();
  void update_kanji_reading_lookup_action();
  void update_kanji_lookup_action();
  void show_edict_results_window(bool show = true);
  void update_kanji_color_actions();
  void update_kana_input_state();
  void set_input_mode(InputMode mode);
  void apply_kana_input_events(
      const std::vector<core::KanaInputEvent>& events);
  void finish_kana_input();
  void reset_kana_input(bool disable_mode);
  bool attempt_automatic_conversion(bool force);
  void clear_automatic_conversion_range();
  void show_automatic_conversion_range();
  void restore_jwp_conversion_state();
  void rollback_conversion_noexcept() noexcept;
  void save_wnn_preferences();
  void show_wnn_user_dictionary_dialog();
  void show_edict_lookup_dialog();
  void show_edict_user_dictionary_dialog();
  void show_kanji_info_dialog(std::optional<CharacterTarget> target = {},
                             std::optional<core::JisCode> code = {});
  void show_kanji_info_code(core::JisCode code);
  void show_jis_table_dialog();
  void show_kanji_count_dialog();
  void show_kanji_code_lookup_dialog(KanjiCodeLookupMode mode);
  void show_kanji_reading_lookup_dialog();
  void show_kanji_lookup_dialog();
  std::optional<core::JisCode> jwp_character_target() const;
  std::u32string edict_query_seed() const;
  void new_document();
  void connect_editor(JwpEditor* editor);
  void refresh_document_view();
  bool finish_document_input();
  int find_document_path(const QString& path) const;
  void record_recent_document(const DocumentState& state);
  void record_recent_file(RecentDocument entry);
  void update_recent_file_actions();
  void configure_application_settings(bool dictionary_page = false);
  void import_query_history_dialog();
  bool confirm_query_history_change(const QString& message, OpenMode mode);
  void check_history_destination(const QString& path) const;
  bool query_history_error(const QString& action, const std::exception& error, OpenMode mode);
  core::LegacyCodePage default_jwp_code_page() const noexcept;
  void open_document();
  bool open_project_dialog(const QString& path = {});
  void save_project_dialog();
  bool save_document();
  bool save_document_as(bool export_copy = false);
  bool maybe_save();
  bool approve_close_all(OpenMode mode);
  std::optional<core::TextEncoding> choose_encoding();
  void load_document(const QString& path, const core::TextFile& file,
                     bool japanese_editing = true, bool new_tab = false);
  bool native_document_modified() const;
  bool confirm_text_export();
  void load_jwp_document(const QString& path, core::JwpDocument document,
                         core::LegacyCodePage code_page, bool new_tab = false);
  void set_text_encoding(core::TextEncoding encoding, bool mark_modified);
  void set_jwp_code_page(core::LegacyCodePage code_page);
  void apply_jwp_presentation(const core::JwpDocument& document,
                              core::LegacyCodePage code_page);
  void clear_jwp_presentation();
  void find_document();
  void show_find_replace(bool replacing, const QString& text, const QString& replacement);
  FindReplaceResult run_find_replace(const FindReplaceRequest& request);
  bool replace_editor_selection(std::u32string_view text);
  void find_again(core::JwpSearchDirection direction);
  void replace_document();
  void format_document_paragraphs();
  void format_file_paragraphs();
  bool apply_paragraph_format(std::size_t first_paragraph,
                              std::size_t last_paragraph,
                              const core::JwpParagraphFormat& format,
                              const QTextCursor& cursor,
                              core::JwpPosition caret);
  void format_page_layout();
  bool apply_page_layout(const core::JwpDocument& requested);
  void print_current_document(bool preview = false);
  void setup_printer();
  void configure_kanji_colors();
  void edit_kanji_color_list();
  bool find_jwp_text(const QString& text, core::JwpSearchOptions options);
  bool find_plain_text(const QString& text, core::JwpSearchOptions options);
  void synchronize_jwp_document(int position, int chars_removed,
                                int chars_added);
  void restore_jwp_editor_text(int cursor_position, int selection_start,
                               int selection_end);
  void update_encoding_display();
  void update_title();
  void show_error(const QString& action, const std::exception& error);

  QTabWidget* document_tabs_;
  QListWidget* conversion_candidates_;
  std::shared_ptr<QPrinter> printer_;
  bool print_busy_ = false;
  bool print_selection_available_ = false;
  QLabel* encoding_label_;
  QAction* undo_action_;
  QAction* redo_action_;
  QAction* cut_action_ = nullptr;
  QAction* copy_action_ = nullptr;
  QAction* next_file_action_ = nullptr;
  QAction* previous_file_action_ = nullptr;
  QList<QAction*> editor_actions_;
  QList<QAction*> recent_file_actions_;
  QAction* clear_recent_files_action_ = nullptr;
  std::vector<RecentDocument> recent_documents_;
  QString recent_files_path_;
  QString recent_file_warning_;
  bool recent_file_persistence_enabled_ = true;
  ApplicationSettings application_settings_;
  QString application_settings_path_;
  QString application_settings_warning_;
  QStringList application_font_warnings_;
  bool application_settings_persistence_enabled_ = true;
  QString project_path_;
  QString project_warning_;
  std::shared_ptr<core::QueryHistories> query_histories_ = std::make_shared<core::QueryHistories>();
  std::optional<QueryHistorySnapshot> query_history_snapshot_;
  QString query_history_path_;
  QString query_history_warning_;
  bool query_history_pruned_ = false;
  bool query_history_busy_ = false;
  QAction* print_action_ = nullptr;
  QAction* printer_setup_action_ = nullptr;
  QAction* revert_action_ = nullptr;
  QAction* delete_action_ = nullptr;
  QAction* convert_action_ = nullptr;
  QAction* previous_candidate_action_ = nullptr;
  QAction* next_candidate_action_ = nullptr;
  QAction* accept_candidate_action_ = nullptr;
  QAction* kana_input_action_ = nullptr;
  QAction* toggle_input_mode_action_ = nullptr;
  QAction* japanese_editing_action_ = nullptr;
  QAction* overwrite_action_ = nullptr;
  QAction* user_dictionary_action_ = nullptr;
  QAction* edict_lookup_action_ = nullptr;
  QAction* edict_results_action_ = nullptr;
  QAction* edict_user_dictionary_action_ = nullptr;
  QAction* kanji_info_action_ = nullptr;
  QAction* jis_table_action_ = nullptr;
  QAction* kanji_count_action_ = nullptr;
  QAction* skip_lookup_action_ = nullptr;
  QAction* four_corner_lookup_action_ = nullptr;
  QAction* bushu_lookup_action_ = nullptr;
  QAction* spahn_lookup_action_ = nullptr;
  QAction* stroke_bushu_lookup_action_ = nullptr;
  QAction* kanji_reading_lookup_action_ = nullptr;
  QAction* index_lookup_action_ = nullptr;
  QAction* kanji_lookup_action_ = nullptr;
  QAction* format_file_action_ = nullptr;
  QAction* format_paragraph_action_ = nullptr;
  QAction* page_layout_action_ = nullptr;
  QAction* insert_page_break_action_ = nullptr;
  QAction* kanji_color_options_action_ = nullptr;
  QAction* make_kanji_color_list_action_ = nullptr;
  QAction* append_kanji_color_list_action_ = nullptr;
  QAction* edit_kanji_color_list_action_ = nullptr;
  QAction* view_kanji_color_list_action_ = nullptr;
  QAction* clear_kanji_color_list_action_ = nullptr;
  QToolButton* input_mode_button_;
  QToolButton* overwrite_button_;
  QToolButton* resource_status_button_;
  QToolBar* main_toolbar_ = nullptr;
  QList<QAction*> toolbar_catalog_;
  std::optional<ToolbarSettings> applied_toolbar_;
  bool updating_toolbar_ = false;
  void apply_toolbar();
  void sync_toolbar_position();
  void customize_toolbar();
  std::vector<std::pair<QAction*, int>> toolbar_icons_;
  std::vector<std::pair<QAction*, QIcon>> toolbar_standard_icons_;
  QActionGroup* input_mode_actions_;
  QActionGroup* encoding_actions_;
  QMenu* jwp_code_page_menu_;
  std::vector<QAction*> jwp_code_page_actions_;
  core::KanjiColorPolicy kanji_color_policy_;
  core::KanjiColorList kanji_color_list_;
  QString kanji_color_settings_path_;
  QString kanji_color_list_path_;
  std::unique_ptr<WnnResources> wnn_resources_;
  WnnUserDictionaryDialog* wnn_user_dictionary_dialog_ = nullptr;
  std::unique_ptr<EdictResourceSet> edict_resources_;
  std::unique_ptr<EdictUserResources> edict_user_resources_;
  QString edict_config_directory_;
  std::shared_ptr<EdictLookupOptions> edict_lookup_options_;
  std::shared_ptr<core::QueryHistory> edict_query_history_;
  EdictLookupDialog* edict_lookup_dialog_ = nullptr;
  EdictResultsWindow* edict_results_window_ = nullptr;
  EdictUserDictionaryDialog* edict_user_dictionary_dialog_ = nullptr;
  std::unique_ptr<core::KanjiInfoDatabase> kanji_info_database_;
  QString kanji_info_path_;
  JisTableDialog* jis_table_dialog_ = nullptr;
  KanjiCountDialog* kanji_count_dialog_ = nullptr;
  KanjiCodeLookupDialog* kanji_code_lookup_dialog_ = nullptr;
  KanjiReadingLookupDialog* kanji_reading_lookup_dialog_ = nullptr;
  std::unique_ptr<core::KanjiLookupLists> radical_lists_;
  std::unique_ptr<core::KanjiLookupLists> stroke_lists_;
  QPixmap radical_sheet_;
  KanjiLookupDialog* kanji_lookup_dialog_ = nullptr;
  QString search_text_;
  QString replacement_text_;
  core::JwpSearchOptions search_options_;
  QPointer<FindReplaceDialog> find_dialog_;
  QPointer<FindReplaceDialog> replace_dialog_;
  bool search_busy_ = false;
  // Conversion transactions must be destroyed before the shared dictionaries.
  std::vector<std::unique_ptr<DocumentState>> documents_;
  DocumentState* document_ = nullptr;
};

}  // namespace jwpqt::qt

#endif
