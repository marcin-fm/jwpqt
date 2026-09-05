// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_MAIN_WINDOW_H
#define JWPQT_QT_MAIN_WINDOW_H

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QMainWindow>
#include <QString>

#include "jwpqt/core/jwp_conversion.h"
#include "jwpqt/core/jwp_document_history.h"
#include "jwpqt/core/jwp_document_model.h"
#include "jwpqt/core/jwp_search.h"
#include "jwpqt/core/kanji_color.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/kana_input.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/text_file.h"
#include "jwpqt/core/wnn_dictionary.h"
#include "jwpqt/core/wnn_preferences.h"
#include "jwpqt/core/wnn_user_dictionary.h"

class QAction;
class QActionGroup;
class QCloseEvent;
class QEvent;
class QLabel;
class QMenu;
class QTextEdit;

namespace jwpqt::qt {

class JwpEditor;
class WnnUserDictionaryDialog;

enum class OpenMode {
  kInteractive,
  kNonInteractive,
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

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  bool open_path(const QString& path, core::TextEncoding encoding,
                 OpenMode mode = OpenMode::kInteractive);
  bool open_jwp_path(
      const QString& path,
      core::LegacyCodePage code_page = core::kDefaultLegacyCodePage,
      OpenMode mode = OpenMode::kInteractive);
  bool open_path_detected(const QString& path,
                          OpenMode mode = OpenMode::kInteractive);
  bool save_path(const QString& path);
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

 protected:
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
  virtual std::optional<core::KanjiColorPolicy>
  prompt_for_kanji_color_policy(const core::KanjiColorPolicy& initial);
  virtual std::optional<KanjiColorListEditRequest>
  prompt_for_kanji_color_list_edit();

 private:
  struct WnnResources;

  void create_actions();
  void undo_document();
  void redo_document();
  void restore_jwp_history_state(core::JwpPosition caret);
  void update_undo_actions();
  void update_conversion_actions();
  void update_kanji_color_actions();
  void update_kana_input_state();
  void set_kana_input_enabled(bool enabled);
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
  void new_document();
  void open_document();
  bool save_document();
  bool save_document_as();
  bool maybe_save();
  std::optional<core::TextEncoding> choose_encoding();
  void load_document(const QString& path, const core::TextFile& file);
  void load_jwp_document(const QString& path, core::JwpDocument document,
                         core::LegacyCodePage code_page);
  void set_text_encoding(core::TextEncoding encoding, bool mark_modified);
  void set_jwp_code_page(core::LegacyCodePage code_page);
  void apply_jwp_presentation(const core::JwpDocument& document,
                              core::LegacyCodePage code_page);
  void clear_jwp_presentation();
  void find_document();
  void find_again(core::JwpSearchDirection direction);
  void replace_document();
  void format_document_paragraphs();
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

  JwpEditor* editor_;
  QLabel* encoding_label_;
  QAction* undo_action_;
  QAction* redo_action_;
  QAction* convert_action_ = nullptr;
  QAction* previous_candidate_action_ = nullptr;
  QAction* next_candidate_action_ = nullptr;
  QAction* accept_candidate_action_ = nullptr;
  QAction* kana_input_action_ = nullptr;
  QAction* user_dictionary_action_ = nullptr;
  QAction* format_paragraph_action_ = nullptr;
  QAction* insert_page_break_action_ = nullptr;
  QAction* kanji_color_options_action_ = nullptr;
  QAction* make_kanji_color_list_action_ = nullptr;
  QAction* append_kanji_color_list_action_ = nullptr;
  QAction* edit_kanji_color_list_action_ = nullptr;
  QAction* view_kanji_color_list_action_ = nullptr;
  QAction* clear_kanji_color_list_action_ = nullptr;
  QLabel* input_mode_label_;
  QActionGroup* encoding_actions_;
  QMenu* jwp_code_page_menu_;
  std::vector<QAction*> jwp_code_page_actions_;
  QString current_path_;
  core::TextEncoding encoding_ = core::TextEncoding::kUtf8;
  bool has_byte_order_mark_ = false;
  std::optional<core::JwpDocumentModel> jwp_document_;
  core::JwpDocumentHistory jwp_history_;
  core::KanjiColorPolicy kanji_color_policy_;
  core::KanjiColorList kanji_color_list_;
  QString kanji_color_settings_path_;
  QString kanji_color_list_path_;
  std::optional<core::JwpPosition> jwp_caret_;
  std::optional<core::JwpPosition> expected_jwp_caret_;
  std::optional<core::JwpDocument> saved_jwp_document_;
  std::optional<core::JwpDocument> pristine_jwp_document_;
  std::u32string rendered_jwp_text_;
  core::LegacyCodePage jwp_code_page_ = core::kDefaultLegacyCodePage;
  std::unique_ptr<WnnResources> wnn_resources_;
  WnnUserDictionaryDialog* wnn_user_dictionary_dialog_ = nullptr;
  std::unique_ptr<core::JwpConversionTransaction> jwp_conversion_;
  std::optional<core::WnnPreferences> conversion_preferences_before_;
  core::KanaInputComposer kana_input_;
  std::optional<core::JwpRange> automatic_conversion_range_;
  bool kana_input_enabled_ = false;
  bool applying_kana_input_ = false;
  QString search_text_;
  QString replacement_text_;
  core::JwpSearchOptions search_options_;
  bool qt_undo_available_ = false;
  bool qt_redo_available_ = false;
  bool updating_editor_ = false;
};

}  // namespace jwpqt::qt

#endif
