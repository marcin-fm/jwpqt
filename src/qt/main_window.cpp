// SPDX-License-Identifier: GPL-2.0-or-later

#include "main_window.h"

#include <algorithm>
#include <array>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include "file_io.h"
#include "jwpqt/core/jwp_plain_text.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/plain_text_change.h"
#include "jwpqt/core/text_detection.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr std::array<core::TextEncoding, 6> kTextEncodings{
    core::TextEncoding::kUtf8,      core::TextEncoding::kEucJp,
    core::TextEncoding::kShiftJis,  core::TextEncoding::kNewJis,
    core::TextEncoding::kOldJis,    core::TextEncoding::kNecJis,
};

constexpr std::array<core::LegacyCodePage, 9> kLegacyCodePages{
    core::LegacyCodePage::k1250, core::LegacyCodePage::k1251,
    core::LegacyCodePage::k1252, core::LegacyCodePage::k1253,
    core::LegacyCodePage::k1254, core::LegacyCodePage::k1255,
    core::LegacyCodePage::k1256, core::LegacyCodePage::k1257,
    core::LegacyCodePage::k1258,
};

QString encoding_name(core::TextEncoding encoding) {
  const std::string_view name = core::text_encoding_name(encoding);
  return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size()));
}

QString code_page_name(core::LegacyCodePage code_page) {
  const std::string_view name = core::legacy_code_page_name(code_page);
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

QString jwp_filter() { return MainWindow::tr("JWP documents (*.jwp)"); }

QString file_filters() {
  QString filters = jwp_filter();
  for (const core::TextEncoding encoding : kTextEncodings) {
    filters += QStringLiteral(";;") + encoding_filter(encoding);
  }
  return filters + QStringLiteral(";;") + MainWindow::tr("All files (*)");
}

QString all_files_filter() { return MainWindow::tr("All files (*)"); }

std::size_t utf32_offset_for_utf16(const QString& text, int offset) {
  if (offset < 0 || offset > text.size()) {
    throw core::PlainTextChangeError("Qt text change offset is out of bounds");
  }
  if (offset > 0 && offset < text.size() &&
      text.at(offset - 1).isHighSurrogate() && text.at(offset).isLowSurrogate()) {
    throw core::PlainTextChangeError(
        "Qt text change splits a Unicode surrogate pair");
  }
  return from_qstring(text.left(offset)).size();
}

int utf16_offset_for_utf32(std::u32string_view text, std::size_t offset) {
  if (offset > text.size()) {
    throw core::JwpSearchError("Search result offset is out of bounds");
  }
  return to_qstring(text.substr(0, offset)).size();
}

QString fold_ascii_case(QString text) {
  for (qsizetype index = 0; index < text.size(); ++index) {
    const ushort value = text.at(index).unicode();
    if (value >= 'A' && value <= 'Z') {
      text[index] = QChar(static_cast<ushort>(value + ('a' - 'A')));
    }
  }
  return text;
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
      encoding_actions_(new QActionGroup(this)),
      jwp_code_page_menu_(nullptr) {
  setCentralWidget(editor_);
  editor_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  editor_->setLineWrapMode(QPlainTextEdit::WidgetWidth);

  create_actions();
  encoding_label_->setObjectName(QStringLiteral("documentEncoding"));
  statusBar()->addPermanentWidget(encoding_label_);
  update_encoding_display();
  resize(900, 680);

  connect(editor_->document(), &QTextDocument::contentsChange, this,
          [this](int position, int chars_removed, int chars_added) {
            synchronize_jwp_document(position, chars_removed, chars_added);
          });
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

  edit_menu->addSeparator();
  QAction* find_action = edit_menu->addAction(tr("&Find..."));
  find_action->setObjectName(QStringLiteral("findAction"));
  find_action->setShortcut(QKeySequence::Find);
  connect(find_action, &QAction::triggered, this,
          [this] { find_document(); });

  QAction* find_next_action = edit_menu->addAction(tr("Find &Next"));
  find_next_action->setObjectName(QStringLiteral("findNextAction"));
  find_next_action->setShortcut(QKeySequence::FindNext);
  connect(find_next_action, &QAction::triggered, this, [this] {
    find_again(core::JwpSearchDirection::kForward);
  });

  QAction* find_previous_action = edit_menu->addAction(tr("Find Pre&vious"));
  find_previous_action->setObjectName(QStringLiteral("findPreviousAction"));
  find_previous_action->setShortcut(QKeySequence::FindPrevious);
  connect(find_previous_action, &QAction::triggered, this, [this] {
    find_again(core::JwpSearchDirection::kBackward);
  });

  QAction* replace_action = edit_menu->addAction(tr("&Replace..."));
  replace_action->setObjectName(QStringLiteral("replaceAction"));
  replace_action->setShortcut(QKeySequence::Replace);
  connect(replace_action, &QAction::triggered, this,
          [this] { replace_document(); });

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

  encoding_menu->addSeparator();
  jwp_code_page_menu_ = encoding_menu->addMenu(tr("J&WP code page"));
  QActionGroup* code_page_group = new QActionGroup(this);
  code_page_group->setExclusive(true);
  for (const core::LegacyCodePage code_page : kLegacyCodePages) {
    QAction* action = jwp_code_page_menu_->addAction(code_page_name(code_page));
    action->setCheckable(true);
    action->setData(static_cast<int>(code_page));
    code_page_group->addAction(action);
    jwp_code_page_actions_.push_back(action);
    connect(action, &QAction::triggered, this,
            [this, code_page] { set_jwp_code_page(code_page); });
  }
}

void MainWindow::new_document() {
  if (!maybe_save()) {
    return;
  }
  jwp_document_.reset();
  saved_jwp_document_.reset();
  pristine_jwp_document_.reset();
  rendered_jwp_text_.clear();
  updating_editor_ = true;
  editor_->clear();
  updating_editor_ = false;
  editor_->document()->setModified(false);
  current_path_.clear();
  has_byte_order_mark_ = false;
  set_text_encoding(core::TextEncoding::kUtf8, false);
  update_encoding_display();
  update_title();
}

void MainWindow::open_document() {
  if (!maybe_save()) {
    return;
  }
  QString selected_filter = all_files_filter();
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open document"), QString(), file_filters(), &selected_filter);
  if (path.isEmpty()) {
    return;
  }
  if (selected_filter == jwp_filter()) {
    open_jwp_path(path, jwp_code_page_);
    return;
  }
  const std::optional<core::TextEncoding> encoding =
      encoding_from_filter(selected_filter);
  if (encoding.has_value()) {
    open_path(path, *encoding);
  } else {
    open_path_detected(path);
  }
}

bool MainWindow::open_path(const QString& path, core::TextEncoding encoding,
                           OpenMode mode) {
  try {
    const core::TextFile file = read_text_file(path, encoding);
    load_document(path, file);
    statusBar()->showMessage(
        tr("Opened %1 as %2").arg(path, encoding_name(encoding_)), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not open %1").arg(path), error);
    }
    return false;
  }
}

bool MainWindow::open_jwp_path(const QString& path,
                               core::LegacyCodePage code_page,
                               OpenMode mode) {
  try {
    load_jwp_document(path, read_jwp_file(path), code_page);
    statusBar()->showMessage(
        tr("Opened %1 as JWP (%2)").arg(path, code_page_name(code_page)),
        3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not open %1").arg(path), error);
    }
    return false;
  }
}

bool MainWindow::open_path_detected(const QString& path, OpenMode mode) {
  try {
    const std::string bytes = read_file_bytes(path);
    if (core::has_jwp_document_magic(bytes)) {
      load_jwp_document(path, core::decode_jwp_document(bytes),
                        jwp_code_page_);
      statusBar()->showMessage(
          tr("Opened %1 as JWP (%2)")
              .arg(path, code_page_name(jwp_code_page_)),
          3000);
      return true;
    }

    const core::TextEncodingDetection detection =
        core::detect_text_encoding(bytes);
    std::optional<core::TextEncoding> encoding;
    if (detection.confidence == core::DetectionConfidence::kCertain &&
        detection.candidates.size() == 1) {
      encoding = detection.candidates.front();
    } else {
      if (mode == OpenMode::kNonInteractive) {
        return false;
      }
      QString explanation;
      switch (detection.confidence) {
        case core::DetectionConfidence::kAmbiguous:
          explanation = tr("Several encodings match this file. Choose one:");
          break;
        case core::DetectionConfidence::kAsciiOnly:
          explanation =
              tr("This file contains only ASCII. Choose its save encoding:");
          break;
        case core::DetectionConfidence::kUnknown:
          explanation = tr("The encoding could not be detected. Choose one:");
          break;
        case core::DetectionConfidence::kCertain:
          explanation = tr("Choose the text encoding:");
          break;
      }
      encoding = prompt_for_encoding(detection.candidates, explanation);
    }
    if (!encoding.has_value()) {
      return false;
    }

    const core::TextFile file = core::decode_text_file(bytes, *encoding);
    load_document(path, file);
    statusBar()->showMessage(
        tr("Opened %1 as %2").arg(path, encoding_name(*encoding)), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not open %1").arg(path), error);
    }
    return false;
  }
}

void MainWindow::load_document(const QString& path,
                               const core::TextFile& file) {
  jwp_document_.reset();
  saved_jwp_document_.reset();
  pristine_jwp_document_.reset();
  rendered_jwp_text_.clear();
  updating_editor_ = true;
  editor_->setPlainText(to_qstring(file.text));
  updating_editor_ = false;
  editor_->document()->setModified(false);
  current_path_ = path;
  encoding_ = file.encoding;
  has_byte_order_mark_ = file.has_byte_order_mark;
  update_encoding_display();
  update_title();
}

void MainWindow::load_jwp_document(const QString& path,
                                    core::JwpDocument document,
                                    core::LegacyCodePage code_page) {
  std::optional<core::JwpDocument> pristine_document;
  if (document.paragraphs.empty()) {
    pristine_document = document;
  }
  core::JwpDocumentModel model(std::move(document));
  std::u32string text = core::decode_jwp_plain_text(model, code_page);

  updating_editor_ = true;
  editor_->setPlainText(to_qstring(text));
  updating_editor_ = false;
  jwp_document_ = std::move(model);
  saved_jwp_document_ = jwp_document_->document();
  pristine_jwp_document_ = std::move(pristine_document);
  rendered_jwp_text_ = std::move(text);
  jwp_code_page_ = code_page;
  current_path_ = path;
  has_byte_order_mark_ = false;
  editor_->document()->setModified(false);
  update_encoding_display();
  update_title();
}

bool MainWindow::save_document() {
  return current_path_.isEmpty() ? save_document_as()
                                 : save_path(current_path_);
}

bool MainWindow::save_document_as() {
  if (is_jwp_document()) {
    QString selected_filter = jwp_filter();
    const QString filters = jwp_filter() + QStringLiteral(";;") +
                            all_files_filter();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save JWP document"), current_path_, filters,
        &selected_filter);
    return !path.isEmpty() && save_path(path);
  }

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
    if (jwp_document_.has_value()) {
      const bool unedited_pristine =
          pristine_jwp_document_.has_value() &&
          saved_jwp_document_.has_value() &&
          jwp_document_->document() == *saved_jwp_document_;
      write_jwp_file(path, unedited_pristine ? *pristine_jwp_document_
                                              : jwp_document_->document());
      if (!unedited_pristine) {
        pristine_jwp_document_.reset();
      }
      saved_jwp_document_ = jwp_document_->document();
      current_path_ = path;
      editor_->document()->setModified(false);
      update_title();
      statusBar()->showMessage(
          tr("Saved %1 as JWP (%2)")
              .arg(path, code_page_name(jwp_code_page_)),
          3000);
      return true;
    }

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
  return prompt_for_encoding(
      std::vector<core::TextEncoding>(kTextEncodings.begin(),
                                      kTextEncodings.end()),
      tr("Encoding:"));
}

std::optional<core::TextEncoding> MainWindow::prompt_for_encoding(
    const std::vector<core::TextEncoding>& candidates,
    const QString& explanation) {
  const std::vector<core::TextEncoding> choices =
      candidates.empty()
          ? std::vector<core::TextEncoding>(kTextEncodings.begin(),
                                            kTextEncodings.end())
          : candidates;
  QStringList names;
  int current_index = 0;
  for (std::size_t index = 0; index < choices.size(); ++index) {
    const core::TextEncoding encoding = choices[index];
    names.append(encoding_name(encoding));
    if (encoding == encoding_) {
      current_index = static_cast<int>(index);
    }
  }
  bool accepted = false;
  const QString selected = QInputDialog::getItem(
      this, tr("Select text encoding"), explanation, names, current_index,
      false, &accepted);
  if (!accepted) {
    return std::nullopt;
  }
  for (const core::TextEncoding encoding : choices) {
    if (selected == encoding_name(encoding)) {
      return encoding;
    }
  }
  return std::nullopt;
}

std::optional<SearchRequest> MainWindow::prompt_for_search(
    const SearchRequest& initial) {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Find"));

  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* text = new QLineEdit(initial.text, &dialog);
  text->setObjectName(QStringLiteral("findText"));
  form->addRow(tr("Find:"), text);
  layout->addLayout(form);

  auto* ignore_case = new QCheckBox(tr("Ignore ASCII case"), &dialog);
  ignore_case->setChecked(initial.options.ignore_ascii_case);
  layout->addWidget(ignore_case);
  auto* jascii = new QCheckBox(tr("Treat full-width ASCII as ASCII"), &dialog);
  jascii->setChecked(initial.options.jascii_ascii_equivalence);
  jascii->setEnabled(is_jwp_document());
  layout->addWidget(jascii);
  auto* wrap = new QCheckBox(tr("Wrap around"), &dialog);
  wrap->setChecked(initial.options.wrap);
  layout->addWidget(wrap);

  auto* direction = new QHBoxLayout();
  auto* forward = new QRadioButton(tr("Forward"), &dialog);
  auto* backward = new QRadioButton(tr("Backward"), &dialog);
  forward->setChecked(initial.options.direction ==
                      core::JwpSearchDirection::kForward);
  backward->setChecked(initial.options.direction ==
                       core::JwpSearchDirection::kBackward);
  direction->addWidget(forward);
  direction->addWidget(backward);
  layout->addLayout(direction);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Find"));
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  text->selectAll();
  text->setFocus();
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return SearchRequest{
      text->text(),
      core::JwpSearchOptions{
          backward->isChecked() ? core::JwpSearchDirection::kBackward
                                : core::JwpSearchDirection::kForward,
          ignore_case->isChecked(), jascii->isChecked(), wrap->isChecked()}};
}

std::optional<ReplaceRequest> MainWindow::prompt_for_replace(
    const ReplaceRequest& initial) {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Replace"));

  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* text = new QLineEdit(initial.text, &dialog);
  text->setObjectName(QStringLiteral("replaceFindText"));
  form->addRow(tr("Find:"), text);
  auto* replacement = new QLineEdit(initial.replacement, &dialog);
  replacement->setObjectName(QStringLiteral("replacementText"));
  form->addRow(tr("Replace with:"), replacement);
  layout->addLayout(form);

  auto* ignore_case = new QCheckBox(tr("Ignore ASCII case"), &dialog);
  ignore_case->setChecked(initial.options.ignore_ascii_case);
  layout->addWidget(ignore_case);
  auto* jascii = new QCheckBox(tr("Treat full-width ASCII as ASCII"), &dialog);
  jascii->setChecked(initial.options.jascii_ascii_equivalence);
  jascii->setEnabled(is_jwp_document());
  layout->addWidget(jascii);
  auto* wrap = new QCheckBox(tr("Wrap around for Replace Next"), &dialog);
  wrap->setChecked(initial.options.wrap);
  layout->addWidget(wrap);

  auto* direction = new QHBoxLayout();
  auto* forward = new QRadioButton(tr("Forward"), &dialog);
  auto* backward = new QRadioButton(tr("Backward"), &dialog);
  forward->setChecked(initial.options.direction ==
                      core::JwpSearchDirection::kForward);
  backward->setChecked(initial.options.direction ==
                       core::JwpSearchDirection::kBackward);
  direction->addWidget(forward);
  direction->addWidget(backward);
  layout->addLayout(direction);

  ReplaceMode mode = initial.mode;
  auto* buttons = new QDialogButtonBox(&dialog);
  QPushButton* replace_button = buttons->addButton(
      tr("Replace Next"), QDialogButtonBox::ActionRole);
  QPushButton* replace_all_button = buttons->addButton(
      tr("Replace All"), QDialogButtonBox::ActionRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  connect(replace_button, &QPushButton::clicked, &dialog, [&] {
    mode = ReplaceMode::kNext;
    dialog.accept();
  });
  connect(replace_all_button, &QPushButton::clicked, &dialog, [&] {
    mode = ReplaceMode::kAll;
    dialog.accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  text->selectAll();
  text->setFocus();
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return ReplaceRequest{
      text->text(), replacement->text(),
      core::JwpSearchOptions{
          backward->isChecked() ? core::JwpSearchDirection::kBackward
                                : core::JwpSearchDirection::kForward,
          ignore_case->isChecked(), jascii->isChecked(), wrap->isChecked()},
      mode};
}

void MainWindow::set_text_encoding(core::TextEncoding encoding,
                                   bool mark_modified) {
  if (jwp_document_.has_value() || encoding_ == encoding) {
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

void MainWindow::set_jwp_code_page(core::LegacyCodePage code_page) {
  if (jwp_code_page_ == code_page) {
    return;
  }
  if (!jwp_document_.has_value()) {
    jwp_code_page_ = code_page;
    update_encoding_display();
    return;
  }
  try {
    std::u32string text =
        core::decode_jwp_plain_text(*jwp_document_, code_page);
    const bool modified = saved_jwp_document_.has_value() &&
                          jwp_document_->document() != *saved_jwp_document_;
    updating_editor_ = true;
    editor_->setPlainText(to_qstring(text));
    updating_editor_ = false;
    rendered_jwp_text_ = std::move(text);
    jwp_code_page_ = code_page;
    editor_->document()->setModified(modified);
    update_encoding_display();
    update_title();
  } catch (const std::exception& error) {
    show_error(tr("Could not use %1").arg(code_page_name(code_page)), error);
    update_encoding_display();
  }
}

void MainWindow::find_document() {
  const std::optional<SearchRequest> request =
      prompt_for_search(SearchRequest{search_text_, search_options_});
  if (request.has_value()) {
    find_text(request->text, request->options);
  }
}

void MainWindow::find_again(core::JwpSearchDirection direction) {
  if (search_text_.isEmpty()) {
    find_document();
    return;
  }
  core::JwpSearchOptions options = search_options_;
  options.direction = direction;
  find_text(search_text_, options);
}

void MainWindow::replace_document() {
  const std::optional<ReplaceRequest> request = prompt_for_replace(
      ReplaceRequest{search_text_, replacement_text_, search_options_});
  if (!request.has_value()) {
    return;
  }
  if (request->mode == ReplaceMode::kAll) {
    replace_all(request->text, request->replacement, request->options);
  } else {
    replace_next(request->text, request->replacement, request->options);
  }
}

bool MainWindow::find_text(const QString& text,
                           core::JwpSearchOptions options) {
  if (text.isEmpty()) {
    statusBar()->showMessage(tr("Enter text to find"), 3000);
    return false;
  }
  search_text_ = text;
  search_options_ = options;
  try {
    return is_jwp_document() ? find_jwp_text(text, options)
                             : find_plain_text(text, options);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not search: %1").arg(QString::fromUtf8(error.what())), 5000);
    return false;
  }
}

bool MainWindow::find_jwp_text(const QString& text,
                               core::JwpSearchOptions options) {
  const core::JwpText pattern =
      core::encode_jwp_text(from_qstring(text), jwp_code_page_);
  const QTextCursor original = editor_->textCursor();
  const int start_utf16 = original.hasSelection() ? original.selectionStart()
                                                  : original.position();
  const std::size_t start_offset =
      utf32_offset_for_utf16(editor_->toPlainText(), start_utf16);
  const core::JwpSearchResult result = core::find_next(
      *jwp_document_, pattern,
      core::jwp_plain_text_position(*jwp_document_, start_offset), options);
  if (!result.match.has_value()) {
    statusBar()->showMessage(tr("Text not found"), 3000);
    return false;
  }

  const std::size_t begin =
      core::jwp_plain_text_offset(*jwp_document_, result.match->begin);
  const std::size_t end =
      core::jwp_plain_text_offset(*jwp_document_, result.match->end);
  QTextCursor found = editor_->textCursor();
  found.setPosition(utf16_offset_for_utf32(rendered_jwp_text_, begin));
  found.setPosition(utf16_offset_for_utf32(rendered_jwp_text_, end),
                    QTextCursor::KeepAnchor);
  editor_->setTextCursor(found);
  statusBar()->showMessage(result.wrapped ? tr("Search wrapped")
                                         : tr("Match found"),
                           2000);
  return true;
}

bool MainWindow::find_plain_text(const QString& text,
                                 core::JwpSearchOptions options) {
  const QTextCursor original = editor_->textCursor();
  const QString source = options.ignore_ascii_case
                             ? fold_ascii_case(editor_->toPlainText())
                             : editor_->toPlainText();
  const QString pattern =
      options.ignore_ascii_case ? fold_ascii_case(text) : text;
  const bool backward =
      options.direction == core::JwpSearchDirection::kBackward;
  const qsizetype start = original.hasSelection() ? original.selectionStart()
                                                  : original.position();
  qsizetype match = -1;
  if (backward && start > 0) {
    match = source.lastIndexOf(pattern, start - 1);
  } else if (!backward && start < source.size()) {
    match = source.indexOf(pattern, start + 1);
  }
  bool wrapped = false;
  if (match < 0 && options.wrap) {
    const qsizetype candidate =
        backward ? source.lastIndexOf(pattern) : source.indexOf(pattern);
    if ((backward && candidate > start) || (!backward && candidate < start)) {
      match = candidate;
      wrapped = true;
    }
  }
  if (match < 0) {
    editor_->setTextCursor(original);
    statusBar()->showMessage(tr("Text not found"), 3000);
    return false;
  }

  QTextCursor found = original;
  found.setPosition(static_cast<int>(match));
  found.setPosition(static_cast<int>(match + text.size()),
                    QTextCursor::KeepAnchor);
  editor_->setTextCursor(found);
  statusBar()->showMessage(wrapped ? tr("Search wrapped")
                                   : tr("Match found"),
                           2000);
  return true;
}

bool MainWindow::replace_next(const QString& text,
                              const QString& replacement,
                              core::JwpSearchOptions options) {
  replacement_text_ = replacement;
  if (text.isEmpty()) {
    statusBar()->showMessage(tr("Enter text to replace"), 3000);
    return false;
  }

  const QTextCursor original = editor_->textCursor();
  try {
    if (is_jwp_document()) {
      core::JwpDocumentModel validation;
      core::replace_jwp_plain_text(validation, 0, 0,
                                   from_qstring(replacement), jwp_code_page_);
    }
    if (!find_text(text, options)) {
      return false;
    }
    QTextCursor match = editor_->textCursor();
    std::optional<core::JwpDocument> expected_jwp;
    if (is_jwp_document()) {
      const QString current = editor_->toPlainText();
      const std::size_t begin =
          utf32_offset_for_utf16(current, match.selectionStart());
      const std::size_t end =
          utf32_offset_for_utf16(current, match.selectionEnd());
      core::JwpDocumentModel candidate(jwp_document_->document());
      core::replace_jwp_plain_text(candidate, begin, end - begin,
                                   from_qstring(replacement), jwp_code_page_);
      expected_jwp = candidate.document();
    }

    match.insertText(replacement);
    if (expected_jwp.has_value() &&
        (!jwp_document_.has_value() ||
         jwp_document_->document() != *expected_jwp)) {
      throw core::JwpPlainTextError(
          "Native editor did not apply the validated JWP replacement");
    }
    statusBar()->showMessage(tr("Replaced one match"), 2000);
    return true;
  } catch (const std::exception& error) {
    editor_->setTextCursor(original);
    statusBar()->showMessage(
        tr("Could not replace: %1").arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

std::size_t MainWindow::replace_all(const QString& text,
                                    const QString& replacement,
                                    core::JwpSearchOptions options) {
  replacement_text_ = replacement;
  if (text.isEmpty()) {
    statusBar()->showMessage(tr("Enter text to replace"), 3000);
    return 0;
  }

  try {
    std::vector<std::pair<int, int>> matches;
    std::optional<core::JwpDocumentModel> candidate_jwp;
    std::u32string expected_jwp_text;
    if (is_jwp_document()) {
      const core::JwpText pattern =
          core::encode_jwp_text(from_qstring(text), jwp_code_page_);
      const std::vector<core::JwpRange> ranges =
          core::find_all(*jwp_document_, pattern, options);
      candidate_jwp.emplace(jwp_document_->document());
      for (auto range = ranges.rbegin(); range != ranges.rend(); ++range) {
        const std::size_t begin =
            core::jwp_plain_text_offset(*jwp_document_, range->begin);
        const std::size_t end =
            core::jwp_plain_text_offset(*jwp_document_, range->end);
        core::replace_jwp_plain_text(*candidate_jwp, begin, end - begin,
                                     from_qstring(replacement),
                                     jwp_code_page_);
        matches.emplace_back(
            utf16_offset_for_utf32(rendered_jwp_text_, begin),
            utf16_offset_for_utf32(rendered_jwp_text_, end));
      }
      expected_jwp_text =
          core::decode_jwp_plain_text(*candidate_jwp, jwp_code_page_);
    } else {
      const QString source = options.ignore_ascii_case
                                 ? fold_ascii_case(editor_->toPlainText())
                                 : editor_->toPlainText();
      const QString pattern =
          options.ignore_ascii_case ? fold_ascii_case(text) : text;
      qsizetype offset = 0;
      while (offset <= source.size() - pattern.size()) {
        const qsizetype match = source.indexOf(pattern, offset);
        if (match < 0) {
          break;
        }
        matches.emplace_back(static_cast<int>(match),
                             static_cast<int>(match + pattern.size()));
        offset = match + pattern.size();
      }
      std::reverse(matches.begin(), matches.end());
    }

    if (matches.empty()) {
      statusBar()->showMessage(tr("Text not found"), 3000);
      return 0;
    }
    const QString original_text = editor_->toPlainText();
    const QTextCursor original_cursor = editor_->textCursor();
    const bool original_modified = editor_->document()->isModified();
    {
      QScopedValueRollback<bool> update_guard(updating_editor_,
                                               candidate_jwp.has_value());
      QTextCursor edit(editor_->document());
      for (const auto& [begin, end] : matches) {
        edit.setPosition(begin);
        edit.setPosition(end, QTextCursor::KeepAnchor);
        edit.insertText(replacement);
      }
      if (candidate_jwp.has_value() &&
          editor_->toPlainText() != to_qstring(expected_jwp_text)) {
        editor_->setPlainText(original_text);
        editor_->setTextCursor(original_cursor);
        editor_->document()->setModified(original_modified);
        throw core::JwpPlainTextError(
            "Native editor did not apply the validated JWP replacements");
      }
    }
    if (candidate_jwp.has_value()) {
      const bool modified = saved_jwp_document_.has_value() &&
                            candidate_jwp->document() != *saved_jwp_document_;
      jwp_document_.emplace(std::move(*candidate_jwp));
      rendered_jwp_text_ = std::move(expected_jwp_text);
      editor_->document()->setModified(modified);
      update_title();
    }
    search_text_ = text;
    search_options_ = options;
    statusBar()->showMessage(tr("Replaced %1 matches").arg(matches.size()),
                             3000);
    return matches.size();
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not replace: %1").arg(QString::fromUtf8(error.what())),
        5000);
    return 0;
  }
}

void MainWindow::synchronize_jwp_document(int position, int chars_removed,
                                          int chars_added) {
  if (updating_editor_ || !jwp_document_.has_value()) {
    return;
  }

  const int cursor_position = editor_->textCursor().position();
  int rejected_selection_start = -1;
  int rejected_selection_end = -1;
  try {
    const QString current_qt = editor_->toPlainText();
    std::u32string current = from_qstring(current_qt);
    if (current == rendered_jwp_text_) {
      return;
    }

    const std::size_t prefix = utf32_offset_for_utf16(current_qt, position);
    const std::size_t replacement_end =
        utf32_offset_for_utf16(current_qt, position + chars_added);
    const std::size_t removed_length =
        static_cast<std::size_t>(chars_removed);
    const std::size_t replacement_length = replacement_end - prefix;
    current = core::replace_plain_text_snapshot(
        rendered_jwp_text_, current, prefix, removed_length,
        std::u32string_view(current).substr(prefix, replacement_length));
    if (chars_removed != 0) {
      rejected_selection_start = position;
      rejected_selection_end = position + chars_removed;
    }

    core::JwpDocumentModel updated = *jwp_document_;
    core::replace_jwp_plain_text(
        updated, prefix, removed_length,
        std::u32string_view(current).substr(
            prefix, replacement_length),
        jwp_code_page_);
    const bool modified = !saved_jwp_document_.has_value() ||
                          updated.document() != *saved_jwp_document_;
    jwp_document_ = std::move(updated);
    rendered_jwp_text_ = std::move(current);
    editor_->document()->setModified(modified);
  } catch (const std::exception& error) {
    restore_jwp_editor_text(cursor_position, rejected_selection_start,
                            rejected_selection_end);
    statusBar()->showMessage(
        tr("Edit rejected: %1").arg(QString::fromUtf8(error.what())), 5000);
  }
}

void MainWindow::restore_jwp_editor_text(int cursor_position,
                                         int selection_start,
                                         int selection_end) {
  const bool modified = saved_jwp_document_.has_value() &&
                        jwp_document_->document() != *saved_jwp_document_;
  updating_editor_ = true;
  editor_->undo();
  if (from_qstring(editor_->toPlainText()) != rendered_jwp_text_) {
    editor_->setPlainText(to_qstring(rendered_jwp_text_));
    QTextCursor cursor = editor_->textCursor();
    cursor.setPosition(
        std::min(cursor_position, editor_->document()->characterCount() - 1));
    editor_->setTextCursor(cursor);
  }
  if (selection_start >= 0 && selection_end >= selection_start) {
    QTextCursor cursor = editor_->textCursor();
    cursor.setPosition(selection_start);
    cursor.setPosition(selection_end, QTextCursor::KeepAnchor);
    editor_->setTextCursor(cursor);
  }
  updating_editor_ = false;
  editor_->document()->setModified(modified);
  update_title();
}

void MainWindow::update_encoding_display() {
  const bool jwp = jwp_document_.has_value();
  encoding_label_->setText(
      jwp ? tr("JWP / %1").arg(code_page_name(jwp_code_page_))
          : encoding_name(encoding_));
  for (QAction* action : encoding_actions_->actions()) {
    action->setEnabled(!jwp);
    action->setChecked(!jwp && action->data().toInt() ==
                                   static_cast<int>(encoding_));
  }
  if (jwp_code_page_menu_ != nullptr) {
    jwp_code_page_menu_->setEnabled(true);
  }
  for (QAction* action : jwp_code_page_actions_) {
    action->setChecked(action->data().toInt() ==
                       static_cast<int>(jwp_code_page_));
  }
}

core::TextEncoding MainWindow::text_encoding() const noexcept {
  return encoding_;
}

bool MainWindow::is_jwp_document() const noexcept {
  return jwp_document_.has_value();
}

core::LegacyCodePage MainWindow::jwp_code_page() const noexcept {
  return jwp_code_page_;
}

const core::JwpDocument* MainWindow::current_jwp_document() const noexcept {
  return jwp_document_.has_value() ? &jwp_document_->document() : nullptr;
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
