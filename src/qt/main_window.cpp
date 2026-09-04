// SPDX-License-Identifier: GPL-2.0-or-later

#include "main_window.h"

#include <array>
#include <exception>
#include <optional>

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTextDocument>

#include "file_io.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr std::array<core::TextEncoding, 6> kTextEncodings{
    core::TextEncoding::kUtf8,      core::TextEncoding::kEucJp,
    core::TextEncoding::kShiftJis,  core::TextEncoding::kNewJis,
    core::TextEncoding::kOldJis,    core::TextEncoding::kNecJis,
};

QString encoding_name(core::TextEncoding encoding) {
  const std::string_view name = core::text_encoding_name(encoding);
  return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size()));
}

QString encoding_filter(core::TextEncoding encoding) {
  switch (encoding) {
    case core::TextEncoding::kUtf8:
      return MainWindow::tr("UTF-8 text (*.txt *.utf8)");
    case core::TextEncoding::kEucJp:
      return MainWindow::tr("EUC-JP text (*.euc)");
    case core::TextEncoding::kShiftJis:
      return MainWindow::tr("Shift-JIS text (*.sjs *.sjis)");
    case core::TextEncoding::kNewJis:
      return MainWindow::tr("New JIS text (*.jis)");
    case core::TextEncoding::kOldJis:
      return MainWindow::tr("Old JIS text (*.old)");
    case core::TextEncoding::kNecJis:
      return MainWindow::tr("NEC JIS text (*.nec)");
  }
  throw core::TextFileError("Unknown text encoding");
}

QString file_filters() {
  return encoding_filter(core::TextEncoding::kUtf8) + QStringLiteral(";;") +
         encoding_filter(core::TextEncoding::kEucJp) + QStringLiteral(";;") +
         encoding_filter(core::TextEncoding::kShiftJis) +
         QStringLiteral(";;") +
         encoding_filter(core::TextEncoding::kNewJis) +
         QStringLiteral(";;") +
         encoding_filter(core::TextEncoding::kOldJis) +
         QStringLiteral(";;") +
         encoding_filter(core::TextEncoding::kNecJis) +
         QStringLiteral(";;") + MainWindow::tr("All files (*)");
}

std::optional<core::TextEncoding> encoding_from_filter(const QString& filter) {
  for (const core::TextEncoding encoding : kTextEncodings) {
    if (filter == encoding_filter(encoding)) {
      return encoding;
    }
  }
  return std::nullopt;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      editor_(new QPlainTextEdit(this)),
      encoding_label_(new QLabel(this)),
      encoding_actions_(new QActionGroup(this)) {
  setCentralWidget(editor_);
  editor_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  editor_->setLineWrapMode(QPlainTextEdit::WidgetWidth);

  create_actions();
  encoding_label_->setObjectName(QStringLiteral("documentEncoding"));
  statusBar()->addPermanentWidget(encoding_label_);
  update_encoding_display();
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

  QMenu* encoding_menu = menuBar()->addMenu(tr("E&ncoding"));
  encoding_actions_->setExclusive(true);
  for (const core::TextEncoding encoding : kTextEncodings) {
    QAction* action = encoding_menu->addAction(encoding_name(encoding));
    action->setCheckable(true);
    action->setData(static_cast<int>(encoding));
    encoding_actions_->addAction(action);
    connect(action, &QAction::triggered, this, [this, encoding] {
      set_text_encoding(encoding, true);
    });
  }
  update_encoding_display();
}

void MainWindow::new_document() {
  if (!maybe_save()) {
    return;
  }
  editor_->clear();
  editor_->document()->setModified(false);
  current_path_.clear();
  has_byte_order_mark_ = false;
  set_text_encoding(core::TextEncoding::kUtf8, false);
  update_title();
}

void MainWindow::open_document() {
  if (!maybe_save()) {
    return;
  }
  QString selected_filter = encoding_filter(encoding_);
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open text file"), QString(), file_filters(), &selected_filter);
  if (path.isEmpty()) {
    return;
  }
  std::optional<core::TextEncoding> encoding =
      encoding_from_filter(selected_filter);
  if (!encoding.has_value()) {
    encoding = choose_encoding();
  }
  if (encoding.has_value()) {
    open_path(path, *encoding);
  }
}

bool MainWindow::open_path(const QString& path,
                           core::TextEncoding encoding) {
  try {
    const core::TextFile file = read_text_file(path, encoding);
    editor_->setPlainText(to_qstring(file.text));
    editor_->document()->setModified(false);
    current_path_ = path;
    encoding_ = file.encoding;
    has_byte_order_mark_ = file.has_byte_order_mark;
    update_encoding_display();
    update_title();
    statusBar()->showMessage(
        tr("Opened %1 as %2").arg(path, encoding_name(encoding_)), 3000);
    return true;
  } catch (const std::exception& error) {
    show_error(tr("Could not open %1").arg(path), error);
    return false;
  }
}

bool MainWindow::save_document() {
  return current_path_.isEmpty() ? save_document_as()
                                 : save_path(current_path_);
}

bool MainWindow::save_document_as() {
  QString selected_filter = encoding_filter(encoding_);
  const QString path = QFileDialog::getSaveFileName(
      this, tr("Save text file"), current_path_, file_filters(),
      &selected_filter);
  if (path.isEmpty()) {
    return false;
  }
  std::optional<core::TextEncoding> encoding =
      encoding_from_filter(selected_filter);
  if (!encoding.has_value()) {
    encoding = choose_encoding();
  }
  if (!encoding.has_value()) {
    return false;
  }
  set_text_encoding(*encoding, true);
  return save_path(path);
}

bool MainWindow::save_path(const QString& path) {
  try {
    write_text_file(path, core::TextFile{
                              from_qstring(editor_->toPlainText()), encoding_,
                              has_byte_order_mark_,
                          });
    current_path_ = path;
    editor_->document()->setModified(false);
    update_title();
    statusBar()->showMessage(
        tr("Saved %1 as %2").arg(path, encoding_name(encoding_)), 3000);
    return true;
  } catch (const std::exception& error) {
    show_error(tr("Could not save %1").arg(path), error);
    return false;
  }
}

std::optional<core::TextEncoding> MainWindow::choose_encoding() {
  QStringList names;
  for (const core::TextEncoding encoding : kTextEncodings) {
    names.append(encoding_name(encoding));
  }
  bool accepted = false;
  const QString selected = QInputDialog::getItem(
      this, tr("Select text encoding"), tr("Encoding:"), names,
      static_cast<int>(encoding_), false, &accepted);
  if (!accepted) {
    return std::nullopt;
  }
  for (const core::TextEncoding encoding : kTextEncodings) {
    if (selected == encoding_name(encoding)) {
      return encoding;
    }
  }
  return std::nullopt;
}

void MainWindow::set_text_encoding(core::TextEncoding encoding,
                                   bool mark_modified) {
  if (encoding_ == encoding) {
    return;
  }
  encoding_ = encoding;
  if (encoding_ != core::TextEncoding::kUtf8) {
    has_byte_order_mark_ = false;
  }
  update_encoding_display();
  if (mark_modified) {
    editor_->document()->setModified(true);
  }
}

void MainWindow::update_encoding_display() {
  encoding_label_->setText(encoding_name(encoding_));
  for (QAction* action : encoding_actions_->actions()) {
    action->setChecked(action->data().toInt() == static_cast<int>(encoding_));
  }
}

core::TextEncoding MainWindow::text_encoding() const noexcept {
  return encoding_;
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
