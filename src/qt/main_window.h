// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_MAIN_WINDOW_H
#define JWPQT_QT_MAIN_WINDOW_H

#include <exception>
#include <optional>

#include <QMainWindow>
#include <QString>

#include "jwpqt/core/text_file.h"

class QActionGroup;
class QCloseEvent;
class QLabel;
class QPlainTextEdit;

namespace jwpqt::qt {

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  bool open_path(const QString& path, core::TextEncoding encoding);
  bool save_path(const QString& path);
  core::TextEncoding text_encoding() const noexcept;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void create_actions();
  void new_document();
  void open_document();
  bool save_document();
  bool save_document_as();
  bool maybe_save();
  std::optional<core::TextEncoding> choose_encoding();
  void set_text_encoding(core::TextEncoding encoding, bool mark_modified);
  void update_encoding_display();
  void update_title();
  void show_error(const QString& action, const std::exception& error);

  QPlainTextEdit* editor_;
  QLabel* encoding_label_;
  QActionGroup* encoding_actions_;
  QString current_path_;
  core::TextEncoding encoding_ = core::TextEncoding::kUtf8;
  bool has_byte_order_mark_ = false;
};

}  // namespace jwpqt::qt

#endif
