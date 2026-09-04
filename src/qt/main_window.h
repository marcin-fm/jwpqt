// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_MAIN_WINDOW_H
#define JWPQT_QT_MAIN_WINDOW_H

#include <exception>

#include <QMainWindow>
#include <QString>

class QCloseEvent;
class QPlainTextEdit;

namespace jwpqt::qt {

class MainWindow : public QMainWindow {
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  bool open_path(const QString& path);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void create_actions();
  void new_document();
  void open_document();
  bool save_document();
  bool save_document_as();
  bool save_to(const QString& path);
  bool maybe_save();
  void update_title();
  void show_error(const QString& action, const std::exception& error);

  QPlainTextEdit* editor_;
  QString current_path_;
  bool has_byte_order_mark_ = false;
};

}  // namespace jwpqt::qt

#endif
