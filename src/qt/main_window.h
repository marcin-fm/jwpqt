// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_MAIN_WINDOW_H
#define JWPQT_QT_MAIN_WINDOW_H

#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <QMainWindow>
#include <QString>

#include "jwpqt/core/jwp_document_model.h"
#include "jwpqt/core/jwp_search.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/text_file.h"

class QActionGroup;
class QCloseEvent;
class QLabel;
class QMenu;
class QPlainTextEdit;

namespace jwpqt::qt {

enum class OpenMode {
  kInteractive,
  kNonInteractive,
};

struct SearchRequest {
  QString text;
  core::JwpSearchOptions options;
};

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  bool open_path(const QString& path, core::TextEncoding encoding,
                 OpenMode mode = OpenMode::kInteractive);
  bool open_jwp_path(
      const QString& path,
      core::LegacyCodePage code_page = core::kDefaultLegacyCodePage,
      OpenMode mode = OpenMode::kInteractive);
  bool open_path_detected(const QString& path,
                          OpenMode mode = OpenMode::kInteractive);
  bool save_path(const QString& path);
  core::TextEncoding text_encoding() const noexcept;
  bool is_jwp_document() const noexcept;
  core::LegacyCodePage jwp_code_page() const noexcept;
  const core::JwpDocument* current_jwp_document() const noexcept;
  bool find_text(const QString& text, core::JwpSearchOptions options = {});

 protected:
  void closeEvent(QCloseEvent* event) override;
  virtual std::optional<core::TextEncoding> prompt_for_encoding(
      const std::vector<core::TextEncoding>& candidates,
      const QString& explanation);
  virtual std::optional<SearchRequest> prompt_for_search(
      const SearchRequest& initial);

 private:
  void create_actions();
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
  void find_document();
  void find_again(core::JwpSearchDirection direction);
  bool find_jwp_text(const QString& text, core::JwpSearchOptions options);
  bool find_plain_text(const QString& text, core::JwpSearchOptions options);
  void synchronize_jwp_document(int position, int chars_removed,
                                int chars_added);
  void restore_jwp_editor_text(int cursor_position, int selection_start,
                               int selection_end);
  void update_encoding_display();
  void update_title();
  void show_error(const QString& action, const std::exception& error);

  QPlainTextEdit* editor_;
  QLabel* encoding_label_;
  QActionGroup* encoding_actions_;
  QMenu* jwp_code_page_menu_;
  std::vector<QAction*> jwp_code_page_actions_;
  QString current_path_;
  core::TextEncoding encoding_ = core::TextEncoding::kUtf8;
  bool has_byte_order_mark_ = false;
  std::optional<core::JwpDocumentModel> jwp_document_;
  std::optional<core::JwpDocument> saved_jwp_document_;
  std::optional<core::JwpDocument> pristine_jwp_document_;
  std::u32string rendered_jwp_text_;
  core::LegacyCodePage jwp_code_page_ = core::kDefaultLegacyCodePage;
  QString search_text_;
  core::JwpSearchOptions search_options_;
  bool updating_editor_ = false;
};

}  // namespace jwpqt::qt

#endif
