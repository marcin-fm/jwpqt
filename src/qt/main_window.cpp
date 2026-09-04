// SPDX-License-Identifier: GPL-2.0-or-later

#include "main_window.h"

#include <exception>

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTextDocument>

#include "jwpqt/core/utf8.h"
#include "file_io.h"
#include "text_bridge.h"

namespace jwpqt::qt {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), editor_(new QPlainTextEdit(this)) {
  setCentralWidget(editor_);
  editor_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  editor_->setLineWrapMode(QPlainTextEdit::WidgetWidth);

  create_actions();
  statusBar()->showMessage(tr("UTF-8"));
  resize(900, 680);

  connect(editor_->document(), &QTextDocument::modificationChanged, this,
          [this] { update_title(); });
  update_title();
}

void MainWindow::create_actions() {
  QMenu* file_menu = menuBar()->addMenu(tr("&File"));

  QAction* new_action = file_menu->addAction(tr("&New"));
  new_action->setShortcut(QKeySequence::New);
  connect(new_action, &QAction::triggered, this,
          [this] { new_document(); });

  QAction* open_action = file_menu->addAction(tr("&Open..."));
  open_action->setShortcut(QKeySequence::Open);
  connect(open_action, &QAction::triggered, this,
          [this] { open_document(); });

  QAction* save_action = file_menu->addAction(tr("&Save"));
  save_action->setShortcut(QKeySequence::Save);
  connect(save_action, &QAction::triggered, this,
          [this] { save_document(); });

  QAction* save_as_action = file_menu->addAction(tr("Save &As..."));
  save_as_action->setShortcut(QKeySequence::SaveAs);
  connect(save_as_action, &QAction::triggered, this,
          [this] { save_document_as(); });

  file_menu->addSeparator();
  QAction* quit_action = file_menu->addAction(tr("&Quit"));
  quit_action->setShortcut(QKeySequence::Quit);
  connect(quit_action, &QAction::triggered, this, &QWidget::close);

  QMenu* edit_menu = menuBar()->addMenu(tr("&Edit"));

  QAction* undo_action = edit_menu->addAction(tr("&Undo"));
  undo_action->setShortcut(QKeySequence::Undo);
  undo_action->setEnabled(false);
  connect(undo_action, &QAction::triggered, editor_, &QPlainTextEdit::undo);
  connect(editor_, &QPlainTextEdit::undoAvailable, undo_action,
          &QAction::setEnabled);

  QAction* redo_action = edit_menu->addAction(tr("&Redo"));
  redo_action->setShortcut(QKeySequence::Redo);
  redo_action->setEnabled(false);
  connect(redo_action, &QAction::triggered, editor_, &QPlainTextEdit::redo);
  connect(editor_, &QPlainTextEdit::redoAvailable, redo_action,
          &QAction::setEnabled);

  edit_menu->addSeparator();
  QAction* cut_action = edit_menu->addAction(tr("Cu&t"));
  cut_action->setShortcut(QKeySequence::Cut);
  cut_action->setEnabled(false);
  connect(cut_action, &QAction::triggered, editor_, &QPlainTextEdit::cut);
  connect(editor_, &QPlainTextEdit::copyAvailable, cut_action,
          &QAction::setEnabled);

  QAction* copy_action = edit_menu->addAction(tr("&Copy"));
  copy_action->setShortcut(QKeySequence::Copy);
  copy_action->setEnabled(false);
  connect(copy_action, &QAction::triggered, editor_, &QPlainTextEdit::copy);
  connect(editor_, &QPlainTextEdit::copyAvailable, copy_action,
          &QAction::setEnabled);

  QAction* paste_action = edit_menu->addAction(tr("&Paste"));
  paste_action->setShortcut(QKeySequence::Paste);
  connect(paste_action, &QAction::triggered, editor_, &QPlainTextEdit::paste);

  edit_menu->addSeparator();
  QAction* select_all_action = edit_menu->addAction(tr("Select &All"));
  select_all_action->setShortcut(QKeySequence::SelectAll);
  connect(select_all_action, &QAction::triggered, editor_,
          &QPlainTextEdit::selectAll);
}

void MainWindow::new_document() {
  if (!maybe_save()) {
    return;
  }
  editor_->clear();
  editor_->document()->setModified(false);
  current_path_.clear();
  has_byte_order_mark_ = false;
  update_title();
}

void MainWindow::open_document() {
  if (!maybe_save()) {
    return;
  }
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open text file"), QString(),
      tr("Text files (*.txt);;All files (*)"));
  if (!path.isEmpty()) {
    open_path(path);
  }
}

bool MainWindow::open_path(const QString& path) {
  try {
    const core::Utf8File file = read_utf8_file(path);
    editor_->setPlainText(to_qstring(file.text));
    editor_->document()->setModified(false);
    current_path_ = path;
    has_byte_order_mark_ = file.has_byte_order_mark;
    update_title();
    statusBar()->showMessage(tr("Opened %1 as UTF-8").arg(path), 3000);
    return true;
  } catch (const std::exception& error) {
    show_error(tr("Could not open %1").arg(path), error);
    return false;
  }
}

bool MainWindow::save_document() {
  return current_path_.isEmpty() ? save_document_as() : save_to(current_path_);
}

bool MainWindow::save_document_as() {
  const QString path = QFileDialog::getSaveFileName(
      this, tr("Save text file"), current_path_,
      tr("Text files (*.txt);;All files (*)"));
  return !path.isEmpty() && save_to(path);
}

bool MainWindow::save_to(const QString& path) {
  try {
    write_utf8_file(path, core::Utf8File{
                              from_qstring(editor_->toPlainText()),
                              has_byte_order_mark_,
                          });
    current_path_ = path;
    editor_->document()->setModified(false);
    update_title();
    statusBar()->showMessage(tr("Saved %1 as UTF-8").arg(path), 3000);
    return true;
  } catch (const std::exception& error) {
    show_error(tr("Could not save %1").arg(path), error);
    return false;
  }
}

bool MainWindow::maybe_save() {
  if (!editor_->document()->isModified()) {
    return true;
  }

  const QMessageBox::StandardButton choice = QMessageBox::warning(
      this, tr("Unsaved changes"),
      tr("The document has changed. Do you want to save it?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (choice == QMessageBox::Save) {
    return save_document();
  }
  return choice == QMessageBox::Discard;
}

void MainWindow::update_title() {
  const QString name = current_path_.isEmpty()
                           ? tr("Untitled")
                           : QFileInfo(current_path_).fileName();
  setWindowTitle(tr("%1[*] - jwpqt").arg(name));
  setWindowModified(editor_->document()->isModified());
}

void MainWindow::show_error(const QString& action,
                            const std::exception& error) {
  QMessageBox::critical(this, tr("jwpqt"),
                        action + QStringLiteral("\n\n") +
                            QString::fromUtf8(error.what()));
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (maybe_save()) {
    event->accept();
  } else {
    event->ignore();
  }
}

}  // namespace jwpqt::qt
