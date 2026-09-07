// SPDX-License-Identifier: GPL-2.0-or-later

#include "main_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPalette>
#include <QPageSetupDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QTextEdit>
#include <QToolButton>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QScopedValueRollback>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QTextCursor>
#include <QTextDocument>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

#include "application_settings_dialog.h"
#include "edict_lookup_dialog.h"
#include "edict_resource_search.h"
#include "edict_results_window.h"
#include "edict_resources.h"
#include "edict_user_dictionary_dialog.h"
#include "file_io.h"
#include "jis_table_dialog.h"
#include "japanese_fonts.h"
#include "jwp_editor.h"
#include "kanji_code_lookup_dialog.h"
#include "kanji_count_dialog.h"
#include "kanji_info_dialog.h"
#include "kanji_info_options_dialog.h"
#include "kanji_lookup_dialog.h"
#include "kanji_reading_lookup_dialog.h"
#include "kanji_color_settings.h"
#include "page_layout_dialog.h"
#include "print_document.h"
#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/jis_table.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/jwp_configuration.h"
#include "jwpqt/core/jwp_plain_text.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/plain_text_change.h"
#include "jwpqt/core/text_detection.h"
#include "text_bridge.h"
#include "wnn_user_dictionary_dialog.h"

namespace jwpqt::qt {
namespace {

constexpr std::array<core::TextEncoding, 10> kTextEncodings{
    core::TextEncoding::kUtf8,      core::TextEncoding::kUtf7,
    core::TextEncoding::kUtf16Le,   core::TextEncoding::kUtf16Be,
    core::TextEncoding::kJfc,       core::TextEncoding::kEucJp,
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
    case core::TextEncoding::kUtf7:
      return MainWindow::tr("UTF-7 text (*.txt *.utf)");
    case core::TextEncoding::kUtf16Le:
      return MainWindow::tr("UTF-16LE text (*.txt *.utf16)");
    case core::TextEncoding::kUtf16Be:
      return MainWindow::tr("UTF-16BE text (*.txt *.utf16)");
    case core::TextEncoding::kJfc:
      return MainWindow::tr("JFC text (*.jfc)");
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

QString project_filter() { return MainWindow::tr("JWP projects (*.jpr)"); }

bool project_magic(std::string_view bytes) {
  return bytes.size() >= 4 && core::ByteReader(bytes).read_u32_le() == core::kJwpProjectMagic;
}

QString file_filters() {
  QString filters = jwp_filter();
  for (const core::TextEncoding encoding : kTextEncodings) {
    filters += QStringLiteral(";;") + encoding_filter(encoding);
  }
  return filters + QStringLiteral(";;") + MainWindow::tr("All files (*)");
}

QString all_files_filter() { return MainWindow::tr("All files (*)"); }

QString absolute_document_path(const QString& path) {
  // Cleaning ".." before resolving directory symlinks changes the target file.
  return path.isEmpty() || QDir::isAbsolutePath(path)
             ? path : QDir::currentPath() + QLatin1Char('/') + path;
}

QString document_path_identity(const QString& path) {
  const QString canonical = QFileInfo(path).canonicalFilePath();
  return canonical.isEmpty() ? absolute_document_path(path) : canonical;
}

QString decode_registry_text(const core::EdictRegistry& registry,
                             std::u16string_view text,
                             core::LegacyCodePage code_page) {
  if (registry.wire_encoding ==
      core::EdictRegistryWireEncoding::kUtf16Le) {
    return QString::fromStdU16String(std::u16string(text));
  }
  std::u32string decoded;
  decoded.reserve(text.size());
  for (const char16_t unit : text) {
    if (unit > 0xffU) {
      throw std::runtime_error(
          "ANSI dictionary registry field contains a non-byte code unit");
    }
    const auto code_point = core::legacy_byte_to_unicode(
        static_cast<std::uint8_t>(unit), code_page);
    if (!code_point.has_value()) {
      throw std::runtime_error(
          "ANSI dictionary registry field contains an undefined byte");
    }
    decoded.push_back(*code_point);
  }
  return to_qstring(decoded);
}

std::size_t ensure_edict_user_entry(core::EdictRegistry& registry) {
  for (std::size_t i = 0; i < registry.entries.size(); ++i) {
    if (registry.entries[i].special == core::EdictRegistrySpecial::kUser) {
      const core::EdictRegistryEntry& entry = registry.entries[i];
      if (entry.encoding != core::EdictRegistryEncoding::kMixed ||
          entry.indexed) {
        throw std::runtime_error(
            "Editable user dictionary must be mixed and unindexed");
      }
      return i;
    }
  }

  core::EdictRegistryEntry entry;
  entry.label = u"User";
  entry.path = u"user.dct";
  entry.encoding = core::EdictRegistryEncoding::kMixed;
  entry.names = core::EdictRegistryNames::kNames;
  entry.special = core::EdictRegistrySpecial::kUser;
  entry.searched = true;
  entry.keep = true;
  entry.quiet = true;
  registry.entries.push_back(std::move(entry));
  return registry.entries.size() - 1;
}

QString resolve_registry_path(const QString& path,
                              const QString& config_directory) {
  if (path.isEmpty()) {
    throw std::runtime_error("Dictionary path is empty");
  }
  return QDir::isAbsolutePath(path)
             ? QDir::cleanPath(path)
             : QDir::cleanPath(QDir(config_directory).absoluteFilePath(path));
}

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

struct MainWindow::DocumentState {
  explicit DocumentState(QWidget* parent) : editor_(new JwpEditor(parent)) {
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(16);
    editor_->setFont(font);
    assign_japanese_font(*editor_, JapaneseFontRole::kFile);
  }

  JwpEditor* editor_;
  QString current_path_;
  core::TextEncoding encoding_ = core::TextEncoding::kUtf8;
  bool has_byte_order_mark_ = false;
  bool jwp_format_ = true;
  std::optional<core::TextFile> saved_text_file_;
  std::optional<core::JwpDocumentModel> jwp_document_;
  core::JwpDocumentHistory jwp_history_;
  std::optional<core::JwpPosition> jwp_caret_;
  std::optional<core::JwpPosition> expected_jwp_caret_;
  std::optional<core::JwpDocument> saved_jwp_document_;
  std::optional<core::JwpDocument> pristine_jwp_document_;
  std::u32string rendered_jwp_text_;
  core::LegacyCodePage jwp_code_page_ = core::kDefaultLegacyCodePage;
  std::unique_ptr<core::JwpConversionTransaction> jwp_conversion_;
  std::optional<core::WnnPreferences> conversion_preferences_before_;
  core::KanaInputComposer kana_input_;
  std::optional<core::JwpRange> automatic_conversion_range_;
  InputMode input_mode_ = InputMode::kKanji;
  bool applying_kana_input_ = false;
  bool qt_undo_available_ = false;
  bool qt_redo_available_ = false;
  bool updating_editor_ = false;
};

struct MainWindow::WnnResources {
  WnnResources(core::WnnDictionary dictionary_value,
               core::WnnPreferences preferences_value,
               core::WnnUserDictionary user_dictionary_value,
               QString preferences_path_value,
               QString user_dictionary_path_value)
      : dictionary(std::move(dictionary_value)),
        preferences(std::move(preferences_value)),
        preferences_path(std::move(preferences_path_value)),
        user_dictionary(std::move(user_dictionary_value)),
        user_dictionary_path(std::move(user_dictionary_path_value)),
        session(dictionary, preferences, user_dictionary.lookup_records()) {}

  core::WnnDictionary dictionary;
  core::WnnPreferences preferences;
  QString preferences_path;
  core::WnnUserDictionary user_dictionary;
  QString user_dictionary_path;
  core::WnnConversionSession session;
};

struct MainWindow::EdictUserResources {
  core::EdictUserDictionary dictionary;
  QString path;
  QString label;
  std::size_t registry_index = 0;
  core::LegacyCodePage code_page = core::kDefaultLegacyCodePage;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      document_tabs_(new QTabWidget(this)),
      conversion_candidates_(new QListWidget(this)),
      printer_(std::make_unique<QPrinter>(QPrinter::HighResolution)),
      encoding_label_(new QLabel(this)),
      undo_action_(nullptr),
      redo_action_(nullptr),
      input_mode_button_(new QToolButton(this)),
      overwrite_button_(new QToolButton(this)),
      resource_status_button_(new QToolButton(this)),
      input_mode_actions_(new QActionGroup(this)),
      encoding_actions_(new QActionGroup(this)),
      jwp_code_page_menu_(nullptr) {
  documents_.push_back(std::make_unique<DocumentState>(this));
  document_ = documents_.front().get();
  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  document_tabs_->setObjectName(QStringLiteral("documentTabs"));
  document_tabs_->setDocumentMode(true);
  document_tabs_->setTabsClosable(true);
  document_tabs_->addTab(document_->editor_, tr("Untitled"));
  layout->addWidget(document_tabs_, 1);
  layout->addWidget(conversion_candidates_);
  setCentralWidget(central);
  QFont content_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  content_font.setPixelSize(16);  // Recovered file/edit/list/bar defaults: k16x16.
  document_->editor_->setFont(content_font);
  document_->editor_->setLineWrapMode(QTextEdit::WidgetWidth);
  conversion_candidates_->setObjectName(QStringLiteral("conversionCandidates"));
  conversion_candidates_->setAccessibleName(tr("Conversion candidates"));
  conversion_candidates_->setFont(content_font);
  assign_japanese_font(*conversion_candidates_, JapaneseFontRole::kKanjiBar, true);
  conversion_candidates_->setFlow(QListView::LeftToRight);
  conversion_candidates_->setWrapping(false);
  conversion_candidates_->setWordWrap(false);
  conversion_candidates_->setResizeMode(QListView::Adjust);
  conversion_candidates_->setMovement(QListView::Static);
  conversion_candidates_->setSpacing(4);
  conversion_candidates_->setFocusPolicy(Qt::NoFocus);
  conversion_candidates_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  conversion_candidates_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  conversion_candidates_->setFixedHeight(
      conversion_candidates_->fontMetrics().height() + 12 +
      style()->pixelMetric(QStyle::PM_ScrollBarExtent));
  conversion_candidates_->hide();
  connect(conversion_candidates_, &QListWidget::currentRowChanged, this,
          [this](int row) {
    if (row < 0 || !conversion_active()) return;
    try {
      document_->jwp_conversion_->select(static_cast<std::size_t>(row));
      restore_jwp_conversion_state();
      document_->editor_->setFocus();
    } catch (const std::exception& error) {
      rollback_conversion_noexcept();
      statusBar()->showMessage(tr("Could not change candidate: %1")
                                  .arg(QString::fromUtf8(error.what())), 5000);
    }
  });

  create_actions();
  connect_editor(document_->editor_);
  connect(document_tabs_, &QTabWidget::currentChanged, this, [this](int index) {
    if (!activate_document(index)) {
      const QSignalBlocker blocker(document_tabs_);
      document_tabs_->setCurrentIndex(current_document_index());
    }
  });
  connect(document_tabs_, &QTabWidget::tabCloseRequested, this,
          [this](int index) { close_document(index); });
  update_command_bar_palette();
  encoding_label_->setObjectName(QStringLiteral("documentEncoding"));
  input_mode_button_->setObjectName(QStringLiteral("inputMode"));
  input_mode_button_->setAutoRaise(true);
  input_mode_button_->setAccessibleName(tr("Input mode"));
  input_mode_button_->setToolTip(
      tr("Click to cycle Kanji, ASCII and JASCII. F4 switches Kanji/ASCII."));
  connect(input_mode_button_, &QToolButton::clicked, this, [this] {
    switch (document_->input_mode_) {
      case InputMode::kKanji: set_input_mode(InputMode::kAscii); break;
      case InputMode::kAscii: set_input_mode(InputMode::kJascii); break;
      case InputMode::kJascii: set_input_mode(InputMode::kKanji); break;
    }
    document_->editor_->setFocus();
  });
  statusBar()->addPermanentWidget(encoding_label_);
  statusBar()->addPermanentWidget(input_mode_button_);
  overwrite_button_->setObjectName(QStringLiteral("overwriteMode"));
  overwrite_button_->setAutoRaise(true);
  overwrite_button_->setCheckable(true);
  overwrite_button_->setFocusPolicy(Qt::NoFocus);
  overwrite_button_->setText(tr("INS"));
  overwrite_button_->setAccessibleName(tr("Insert or overwrite mode"));
  overwrite_button_->setToolTip(tr("Insert: click or press Insert to toggle overwrite"));
  connect(overwrite_button_, &QToolButton::clicked, overwrite_action_, &QAction::trigger);
  statusBar()->addPermanentWidget(overwrite_button_);
  resource_status_button_->setObjectName(QStringLiteral("resourceStatus"));
  resource_status_button_->setAutoRaise(true);
  resource_status_button_->setAccessibleName(tr("Runtime resources"));
  statusBar()->addPermanentWidget(resource_status_button_);
  update_resource_status();
  update_encoding_display();
  resize(900, 680);

  new_document();
}

void MainWindow::connect_editor(JwpEditor* editor) {
  editor->setOverwriteMode(overwrite_action_->isChecked());
  editor->installEventFilter(this);
  editor->viewport()->installEventFilter(this);
  // Shared actions route through the active state, never a retired editor.
  editor->setContextMenuPolicy(Qt::ActionsContextMenu);
  editor->addActions(editor_actions_);
  connect(editor->document(), &QTextDocument::contentsChange, this,
          [this, editor](int position, int chars_removed, int chars_added) {
            if (document_->editor_ == editor)
              synchronize_jwp_document(position, chars_removed, chars_added);
          });
  connect(editor->document(), &QTextDocument::modificationChanged, this,
          [this, editor] { if (document_->editor_ == editor) update_title(); });
  connect(editor, &QTextEdit::cursorPositionChanged, this, [this, editor] {
    if (document_->editor_ != editor) return;
    if (!document_->updating_editor_) update_kanji_info_action();
    if (document_->updating_editor_ || conversion_active() ||
        !document_->jwp_document_.has_value()) {
      return;
    }
    try {
      const std::size_t offset = utf32_offset_for_utf16(
          document_plain_text(*document_->editor_->document()), document_->editor_->textCursor().position());
      const core::JwpPosition caret =
          core::jwp_plain_text_position(*document_->jwp_document_, offset);
      if (document_->expected_jwp_caret_.has_value() &&
          caret == *document_->expected_jwp_caret_) {
        document_->expected_jwp_caret_.reset();
      } else {
        document_->jwp_history_.break_coalescing();
        document_->expected_jwp_caret_.reset();
      }
      document_->jwp_caret_ = caret;
      if (document_->automatic_conversion_range_.has_value() &&
          !document_->applying_kana_input_ &&
          caret != document_->automatic_conversion_range_->end) {
        clear_automatic_conversion_range();
      }
    } catch (const std::exception&) {
      document_->jwp_caret_.reset();
      document_->expected_jwp_caret_.reset();
      document_->jwp_history_.break_coalescing();
    }
  });
  connect(editor, &QTextEdit::selectionChanged, this, [this, editor] {
    if (document_->editor_ == editor) update_conversion_actions();
  });
  connect(editor, &QTextEdit::undoAvailable, this, [this, editor](bool available) {
    if (document_->editor_ != editor) return;
    document_->qt_undo_available_ = available;
    update_undo_actions();
  });
  connect(editor, &QTextEdit::redoAvailable, this, [this, editor](bool available) {
    if (document_->editor_ != editor) return;
    document_->qt_redo_available_ = available;
    update_undo_actions();
  });
  connect(editor, &QTextEdit::copyAvailable, this, [this, editor](bool available) {
    if (document_->editor_ != editor) return;
    cut_action_->setEnabled(available);
    copy_action_->setEnabled(available);
  });
}

JwpEditor* MainWindow::active_editor() const noexcept { return document_->editor_; }

int MainWindow::document_count() const noexcept {
  return static_cast<int>(documents_.size());
}

int MainWindow::current_document_index() const noexcept {
  for (int i = 0; i < document_count(); ++i)
    if (documents_[i].get() == document_) return i;
  return -1;
}

bool MainWindow::finish_document_input() {
  if (document_->updating_editor_ || document_->applying_kana_input_) return false;
  if (conversion_active() && !accept_conversion()) return false;
  finish_kana_input();
  if (conversion_active() && !accept_conversion()) return false;
  document_->jwp_history_.break_coalescing();
  clear_automatic_conversion_range();
  return true;
}

bool MainWindow::activate_document(int index) {
  if (index < 0 || index >= document_count()) return false;
  if (documents_[index].get() == document_) return true;
  if (!finish_document_input()) return false;
  document_ = documents_[index].get();
  {
    const QSignalBlocker blocker(document_tabs_);
    document_tabs_->setCurrentIndex(index);
  }
  refresh_document_view();
  return true;
}

void MainWindow::refresh_document_view() {
  if (document_->jwp_document_) {
    const QScopedValueRollback<bool> guard(document_->updating_editor_, true);
    apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
  }
  document_->qt_undo_available_ = document_->editor_->document()->isUndoAvailable();
  document_->qt_redo_available_ = document_->editor_->document()->isRedoAvailable();
  cut_action_->setEnabled(document_->editor_->textCursor().hasSelection());
  copy_action_->setEnabled(document_->editor_->textCursor().hasSelection());
  update_encoding_display();
  update_undo_actions();
  update_conversion_actions();
  update_title();
  document_->editor_->setFocus();
}

int MainWindow::new_document_tab(bool japanese_editing) {
  if (!finish_document_input()) return -1;
  auto next = std::make_unique<DocumentState>(this);
  next->jwp_code_page_ = default_jwp_code_page();
  next->editor_->setLineWrapMode(QTextEdit::WidgetWidth);
  next->editor_->setVerticalScrollBarPolicy(application_settings_.vertical_scrollbar
      ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
  next->editor_->setHorizontalScrollBarPolicy(application_settings_.horizontal_scrollbar
      ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
  connect_editor(next->editor_);
  documents_.push_back(std::move(next));
  {
    const QSignalBlocker blocker(document_tabs_);
    document_tabs_->addTab(documents_.back()->editor_, tr("Untitled"));
  }
  const int index = document_count() - 1;
  if (!activate_document(index)) return -1;
  if (japanese_editing) new_document();
  else load_document({}, core::TextFile{}, false);
  return index;
}

bool MainWindow::close_document(int index, OpenMode mode) {
  if (!activate_document(index)) return false;
  if (mode == OpenMode::kInteractive) {
    if (!maybe_save()) return false;
  } else if (!finish_document_input() || document_modified()) return false;
  if (document_count() == 1) {
    record_recent_document(*document_);
    document_->editor_->document()->setModified(false);
    document_->saved_text_file_.reset();
    new_document();
    return true;
  }
  if (!activate_document(index + 1 < document_count() ? index + 1 : index - 1))
    return false;
  record_recent_document(*documents_[index]);
  const QSignalBlocker blocker(document_tabs_);
  document_tabs_->removeTab(index);
  auto closed = std::move(documents_[index]);
  documents_.erase(documents_.begin() + index);
  delete closed->editor_;
  update_title();
  return true;
}

bool MainWindow::approve_close_all(OpenMode mode) {
  for (int i = 0; i < document_count(); ++i) {
    if (!activate_document(i)) return false;
    if (mode == OpenMode::kInteractive) {
      if (!maybe_save()) return false;
    } else if (!finish_document_input() || document_modified()) return false;
  }
  return true;
}

bool MainWindow::close_all_documents(OpenMode mode) {
  if (!approve_close_all(mode)) return false;
  // Do not discard even the first document until every prompt has succeeded.
  for (const auto& state : documents_) {
    state->editor_->document()->setModified(false);
    if (state->saved_text_file_) {
      state->encoding_ = state->saved_text_file_->encoding;
      state->has_byte_order_mark_ = state->saved_text_file_->has_byte_order_mark;
    }
  }
  for (int i = document_count() - 1; i >= 0; --i)
    if (!close_document(i, OpenMode::kNonInteractive)) return false;
  return true;
}

bool MainWindow::save_all_documents(OpenMode mode) {
  const int original = current_document_index();
  for (int i = 0; i < document_count(); ++i) {
    if (!activate_document(i)) return false;
    if (mode == OpenMode::kInteractive) {
      if (!save_document()) return false;
    } else if (document_->current_path_.isEmpty() ||
               !save_as_path(document_->current_path_,
                             document_->jwp_format_ ? std::nullopt
                                 : std::optional{document_->encoding_},
                             false, false, mode)) return false;
  }
  return activate_document(original);
}

const std::vector<RecentDocument>& MainWindow::recent_documents() const noexcept {
  return recent_documents_;
}

QString MainWindow::recent_file_warning() const { return recent_file_warning_; }

const ApplicationSettings& MainWindow::application_settings() const noexcept {
  return application_settings_;
}

QString MainWindow::application_settings_warning() const {
  return application_settings_warning_;
}

core::LegacyCodePage MainWindow::default_jwp_code_page() const noexcept {
  return static_cast<core::LegacyCodePage>(application_settings_.translation_code_page == 0
      ? 1252 : application_settings_.translation_code_page);
}

QString MainWindow::current_project_path() const { return project_path_; }
QString MainWindow::project_warning() const { return project_warning_; }

bool MainWindow::open_project_path(const QString& path, const ProjectOpenOptions& options, OpenMode mode) {
  try {
    const auto project_bytes = read_file_bytes(path, core::JwpProjectLimits{}.encoded_bytes);
    const auto project = core::parse_jwp_project(project_bytes);
    auto mappings = options.path_mappings;
    ProjectWorkspace workspace;
    for (;;) {
      try {
        workspace = decode_project_workspace(project, path, application_settings_, mappings);
        break;
      } catch (const ProjectPathError& error) {
        if (mode == OpenMode::kNonInteractive) throw;
        const auto directory = QFileDialog::getExistingDirectory(this,
            tr("Linux directory corresponding to %1").arg(error.source_directory()), QFileInfo(path).absolutePath());
        if (directory.isEmpty()) return false;
        mappings.push_back({error.source_directory(), directory});
      }
    }
    if (!workspace.settings.unapplied.empty() && !options.allow_unapplied_settings) {
      if (mode == OpenMode::kNonInteractive)
        throw core::JwpProjectError("Project contains unapplied settings; explicit consent is required");
      QMessageBox warning(QMessageBox::Warning, tr("Project settings"),
          tr("%1 settings will be retained but are not implemented. See details for the list.\n\n"
             "Open the project with the supported settings?").arg(workspace.settings.unapplied.size()),
          QMessageBox::Yes | QMessageBox::Cancel, this);
      warning.setDetailedText(workspace.settings.unapplied.join(QLatin1Char('\n')));
      warning.setDefaultButton(QMessageBox::Cancel);
      if (warning.exec() != QMessageBox::Yes) return false;
    }
    if (options.append) {
      std::size_t count = documents_.size();
      for (const auto& entry : workspace.documents)
        if (find_document_path(entry.path) < 0 && ++count > kMaximumWorkspaceDocuments)
          throw core::JwpProjectError("The combined workspace exceeds the document limit");
    }

    constexpr std::size_t maximum_bytes = 64U * 1024U * 1024U;
    std::vector<std::string> bytes(workspace.documents.size());
    const auto read_documents = [&] {
      std::size_t remaining = maximum_bytes;
      for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (options.append && find_document_path(workspace.documents[i].path) >= 0) continue;
        bytes[i] = read_file_bytes(workspace.documents[i].path, remaining);
        remaining -= bytes[i].size();
      }
    };
    read_documents();
    if (workspace.detect_formats) {
      for (std::size_t i = 0; i < bytes.size(); ++i) {
        auto& entry = workspace.documents[i];
        if (options.append && find_document_path(entry.path) >= 0) continue;
        if (project_magic(bytes[i])) throw core::JwpProjectError("Nested project references are not supported");
        if (core::has_jwp_document_magic(bytes[i])) continue;
        if (QFileInfo(entry.path).suffix().compare(QStringLiteral("jfc"), Qt::CaseInsensitive) == 0)
          entry.encoding = core::TextEncoding::kJfc;
        else {
          const auto detection = core::detect_text_encoding(bytes[i]);
          if (detection.confidence == core::DetectionConfidence::kCertain && detection.candidates.size() == 1)
            entry.encoding = detection.candidates.front();
          else if (options.legacy_encoding) entry.encoding = options.legacy_encoding;
          else if (mode == OpenMode::kInteractive) {
            entry.encoding = prompt_for_encoding(detection.candidates,
                tr("Choose the encoding for project document %1").arg(entry.path));
            if (!entry.encoding) return false;
          } else throw core::JwpProjectError("Project document encoding is ambiguous; select an explicit legacy encoding");
        }
      }
    }

    std::vector<DocumentState*> targets(workspace.documents.size());
    QStringList warnings;
    std::size_t incoming_count = 0;
    // Reuse the normal import/presentation path without changing the live workspace.
    const auto stage_documents = [&] {
      auto staged = std::make_unique<MainWindow>();
      staged->kanji_color_list_ = kanji_color_list_;
      staged->kanji_color_policy_ = kanji_color_policy_;
      if (!staged->apply_application_settings(workspace.settings))
        throw core::JwpProjectError(staged->application_settings_warning().toStdString());
      incoming_count = 0;
      std::size_t characters = 0;
      warnings.clear();
      for (std::size_t i = 0; i < workspace.documents.size(); ++i) {
        const auto& entry = workspace.documents[i];
        const int existing = options.append ? find_document_path(entry.path) : -1;
        if (existing >= 0) {
          targets[i] = documents_[existing].get();
          warnings << tr("Existing buffer and format retained: %1").arg(entry.path);
          continue;
        }
        if (incoming_count != 0 && staged->new_document_tab() < 0)
          throw core::JwpProjectError("Could not prepare a project editor");
        staged->document_->jwp_code_page_ = entry.code_page;
        try {
          if (entry.encoding) {
            staged->load_document(entry.path, core::decode_text_file(bytes[i], *entry.encoding), entry.japanese_editing);
          } else {
            staged->load_jwp_document(entry.path, core::decode_jwp_document(bytes[i]), entry.code_page);
            if (!entry.japanese_editing && !staged->set_japanese_editing(false))
              warnings << tr("Japanese editing retained to preserve document metadata: %1").arg(entry.path);
          }
        } catch (const std::exception& error) {
          throw core::JwpProjectError((entry.path + QStringLiteral(": ") + QString::fromUtf8(error.what())).toStdString());
        }
        if (entry.japanese_editing && !staged->is_jwp_document())
          warnings << tr("Unrestricted Unicode retained for unrepresentable text: %1").arg(entry.path);
        const auto length = static_cast<std::size_t>(staged->document_->editor_->document()->characterCount());
        if (length > 32U * 1024U * 1024U - characters)
          throw core::JwpProjectError("Project text exceeds the workspace character limit");
        characters += length;
        targets[i] = staged->document_;
        ++incoming_count;
      }
      return staged;
    };
    auto staged = stage_documents();
    struct SavedState {
      DocumentState* state;
      bool dirty;
      std::optional<core::TextEncoding> encoding;
      core::LegacyCodePage code_page;
    };
    const auto modified = [](const DocumentState& state) {
      return state.editor_->document()->isModified() || (state.saved_text_file_ &&
          (state.encoding_ != state.saved_text_file_->encoding ||
           state.has_byte_order_mark_ != state.saved_text_file_->has_byte_order_mark));
    };
    std::vector<SavedState> before;
    for (const auto& state : documents_) {
      const auto saved_encoding = state->jwp_format_ ? std::nullopt : std::optional{
          state->saved_text_file_ ? state->saved_text_file_->encoding : state->encoding_};
      before.push_back({state.get(), modified(*state) || state->kana_input_.pending() || state->jwp_conversion_,
                        saved_encoding, state->jwp_code_page_});
    }
    if (!options.append) {
      if (mode == OpenMode::kNonInteractive)
        for (const auto& saved : before) if (saved.dirty)
          throw core::JwpProjectError("Save or close modified documents before replacing the workspace");
      const int original = current_document_index();
      if (!approve_close_all(mode)) { activate_document(original); return false; }
      if (read_file_bytes(path, core::JwpProjectLimits{}.encoded_bytes) != project_bytes)
        throw core::JwpProjectError("The project changed while resolving unsaved documents; open it again");
      const auto previous_bytes = bytes;
      // Save prompts can change both the bytes and their encoding before replacement.
      read_documents();
      QStringList saved_formats;
      for (std::size_t i = 0; i < workspace.documents.size(); ++i) {
        auto& entry = workspace.documents[i];
        for (const auto& saved : before) {
          const auto& state = *saved.state;
          if (!saved.dirty || modified(state) ||
              document_path_identity(state.current_path_) != document_path_identity(entry.path)) continue;
          const auto encoding = state.jwp_format_ ? std::nullopt : std::optional{state.encoding_};
          if (bytes[i] != previous_bytes[i] || encoding != saved.encoding || state.jwp_code_page_ != saved.code_page) {
            entry.encoding = encoding;
            entry.code_page = state.jwp_code_page_;
            entry.japanese_editing = state.jwp_document_.has_value();
            saved_formats << tr("Restored the version just saved during project opening: %1").arg(entry.path);
          }
        }
      }
      staged = stage_documents();
      warnings += saved_formats;
    } else if (!targets.empty() && targets[workspace.current_document] != document_ && !finish_document_input()) return false;

    documents_.reserve((options.append ? documents_.size() : 0) + std::max<std::size_t>(1, incoming_count));
    if (!apply_application_settings(workspace.settings))
      throw core::JwpProjectError(application_settings_warning().toStdString());
    DocumentState* selected = targets.empty() ? (options.append ? document_ : staged->document_)
                                             : targets[workspace.current_document];
    std::vector<std::unique_ptr<DocumentState>> retired;
    if (!options.append) retired.reserve(std::max<std::size_t>(1, incoming_count));
    {
      const QSignalBlocker main_blocker(document_tabs_);
      const QSignalBlocker stage_blocker(staged->document_tabs_);
      if (!options.append) {
        for (const auto& state : documents_) {
          state->editor_->disconnect(this);
          state->editor_->document()->disconnect(this);
          state->editor_->removeEventFilter(this);
          state->editor_->viewport()->removeEventFilter(this);
        }
        while (document_tabs_->count()) document_tabs_->removeTab(0);
        retired.swap(documents_);
      }
      if (incoming_count != 0 || !options.append) {
        // The editors survive staging; all callbacks and actions must change owners.
        for (auto& state : staged->documents_) {
          auto* editor = state->editor_;
          editor->disconnect(staged.get());
          editor->document()->disconnect(staged.get());
          editor->removeEventFilter(staged.get());
          editor->viewport()->removeEventFilter(staged.get());
          for (auto* action : staged->editor_actions_) editor->removeAction(action);
          staged->document_tabs_->removeTab(0);
          editor->setParent(this);
          connect_editor(editor);
          QString title = state->current_path_.isEmpty() ? tr("Untitled") : QFileInfo(state->current_path_).fileName();
          const int tab = document_tabs_->addTab(editor, title.replace(QLatin1Char('&'), QStringLiteral("&&")));
          document_tabs_->setTabToolTip(tab, state->current_path_);
          documents_.push_back(std::move(state));
        }
        staged->documents_.clear();
        staged->document_ = nullptr;
      }
      document_ = selected;
      document_tabs_->setCurrentIndex(current_document_index());
    }
    refresh_document_view();
    project_path_ = absolute_document_path(path);
    project_warning_ = warnings.join(QLatin1Char('\n'));
    for (const auto& state : retired) {
      record_recent_document(*state);
      delete state->editor_;
    }
    for (auto* target : targets) record_recent_document(*target);
    record_recent_file({project_path_, {}, default_jwp_code_page(), true});
    update_resource_status();
    statusBar()->showMessage(tr("Opened project %1 (%2 documents)").arg(path).arg(workspace.documents.size()), 5000);
    return true;
  } catch (const std::exception& error) {
    project_warning_ = tr("Could not open project: %1").arg(QString::fromUtf8(error.what()));
    update_resource_status();
    if (mode == OpenMode::kInteractive) show_error(tr("Could not open project %1").arg(path), error);
    return false;
  }
}

bool MainWindow::save_project_path(const QString& path, bool save_documents, OpenMode mode) {
  try {
    if (path.isEmpty() || find_document_path(path) >= 0)
      throw core::JwpProjectError("A project cannot overwrite an open document");
    const int original = current_document_index();
    ProjectWorkspace workspace;
    workspace.detect_formats = false;
    bool preceding = false;
    for (int i = 0; i < document_count(); ++i) {
      if (save_documents && !activate_document(i)) return false;
      const auto& state = *documents_[i];
      const bool empty = state.current_path_.isEmpty() && !state.editor_->document()->isModified() &&
          !state.kana_input_.pending() && !state.jwp_conversion_ &&
          document_plain_text(*state.editor_->document()).isEmpty();
      if (empty) continue;
      if (save_documents) {
        if (mode == OpenMode::kInteractive) { if (!save_document()) return false; }
        else if (document_->current_path_.isEmpty() ||
                 !save_as_path(document_->current_path_, document_->jwp_format_ ? std::nullopt
                     : std::optional{document_->encoding_}, false, false, mode)) return false;
      }
      if (state.current_path_.isEmpty())
        throw core::JwpProjectError("Save unnamed documents before saving a project");
      if (i <= original) { workspace.current_document = workspace.documents.size(); preceding = true; }
      const auto encoding = state.jwp_format_ ? std::nullopt : std::optional{
          state.saved_text_file_ ? state.saved_text_file_->encoding : state.encoding_};
      workspace.documents.push_back({absolute_document_path(state.current_path_), encoding,
                                     state.jwp_code_page_, state.jwp_document_.has_value()});
    }
    if (!preceding && !workspace.documents.empty()) workspace.current_document = workspace.documents.size() - 1;
    if (save_documents && !activate_document(original)) return false;
    workspace.settings = application_settings_;
    // Save As prompts may have introduced a new collision with the project destination.
    if (find_document_path(path) >= 0) throw core::JwpProjectError("A project cannot overwrite an open document");
    write_jwp_project_file(path, encode_project_workspace(workspace));
    project_path_ = absolute_document_path(path);
    project_warning_.clear();
    record_recent_file({project_path_, {}, default_jwp_code_page(), true});
    update_resource_status();
    statusBar()->showMessage(tr("Saved project %1").arg(path), 3000);
    return true;
  } catch (const std::exception& error) {
    project_warning_ = tr("Could not save project: %1").arg(QString::fromUtf8(error.what()));
    update_resource_status();
    if (mode == OpenMode::kInteractive) show_error(tr("Could not save project %1").arg(path), error);
    return false;
  }
}

bool MainWindow::apply_application_settings(const ApplicationSettings& settings, OpenMode mode) {
  try {
    auto next = read_application_settings(write_application_settings(settings));
    for (const auto& state : documents_)
      if (state->updating_editor_ || state->applying_kana_input_)
        throw core::JwpConfigurationError("Settings cannot change during an editor update");

    {
      struct View {
        DocumentState* state;
        QTextCursor cursor;
        int vertical;
        int horizontal;
        bool modified;
      };
      std::vector<View> views;
      views.reserve(documents_.size());
      for (const auto& state : documents_)
        views.push_back({state.get(), state->editor_->textCursor(),
                         state->editor_->verticalScrollBar()->value(),
                         state->editor_->horizontalScrollBar()->value(),
                         state->editor_->document()->isModified()});
      for (const auto& view : views) view.state->updating_editor_ = true;
      struct RestoreViews {
        std::vector<View>& views;
        ~RestoreViews() {
          for (const auto& view : views) {
            auto* editor = view.state->editor_;
            editor->setTextCursor(view.cursor);
            editor->verticalScrollBar()->setValue(view.vertical);
            editor->horizontalScrollBar()->setValue(view.horizontal);
            editor->document()->setModified(view.modified);
            view.state->updating_editor_ = false;
          }
        }
      } restore{views};
      // Font/layout signals must not be interpreted as edits in any open tab.
      application_font_warnings_ = set_japanese_fonts(*this, next);
      for (const auto& state : documents_) {
        if (state->current_path_.isEmpty() && !state->editor_->document()->isModified() &&
            document_plain_text(*state->editor_->document()).isEmpty() &&
            !state->jwp_history_.can_undo() && !state->jwp_history_.can_redo() &&
            !state->editor_->document()->isUndoAvailable() && !state->editor_->document()->isRedoAvailable())
          state->jwp_code_page_ = static_cast<core::LegacyCodePage>(
              next.translation_code_page == 0 ? 1252 : next.translation_code_page);
        state->editor_->setVerticalScrollBarPolicy(next.vertical_scrollbar
            ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        state->editor_->setHorizontalScrollBarPolicy(next.horizontal_scrollbar
            ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
        if (state->jwp_document_) {
          state->editor_->apply_jwp_layout(state->jwp_document_->document());
          state->editor_->apply_kanji_colors(state->jwp_document_->document(),
              kanji_color_list_, kanji_color_policy_, state->jwp_code_page_);
        }
      }
      application_settings_ = std::move(next);
    }
    main_toolbar_->setVisible(application_settings_.show_toolbar);
    statusBar()->setVisible(application_settings_.show_status_bar);
    auto* layout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    layout->removeWidget(conversion_candidates_);
    layout->insertWidget(application_settings_.kanji_bar_at_top ? 0 : 1, conversion_candidates_);
    conversion_candidates_->setHorizontalScrollBarPolicy(application_settings_.kanji_bar_scrollbar
        ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
    conversion_candidates_->setFixedHeight(conversion_candidates_->fontMetrics().height() + 12 +
        (application_settings_.kanji_bar_scrollbar ? style()->pixelMetric(QStyle::PM_ScrollBarExtent) : 0));
    application_settings_warning_.clear();
    for (auto* child : findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog"),
                                             Qt::FindDirectChildrenOnly))
      if (auto* info = dynamic_cast<KanjiInfoDialog*>(child))
        info->set_options(application_settings_.kanji_info);
    update_encoding_display();
    update_conversion_actions();
    update_undo_actions();
    update_title();
    update_resource_status();
    return true;
  } catch (const std::exception& error) {
    application_settings_warning_ = tr("Could not apply settings: %1").arg(QString::fromUtf8(error.what()));
    update_resource_status();
    if (mode == OpenMode::kInteractive)
      QMessageBox::warning(this, tr("Settings"), application_settings_warning_);
    return false;
  }
}

bool MainWindow::load_application_settings(const QString& path, OpenMode mode) {
  application_settings_path_ = absolute_document_path(path);
  application_settings_persistence_enabled_ = false;
  try {
    if (path.isEmpty()) throw core::JwpConfigurationError("Settings path must not be empty");
    const QFileInfo file(path);
    const auto next = !file.exists() && !file.isSymLink()
        ? ApplicationSettings{} : read_application_settings_file(path);
    if (!apply_application_settings(next, mode)) return false;
    application_settings_persistence_enabled_ = true;
    return true;
  } catch (const std::exception& error) {
    application_settings_warning_ = tr("Could not load settings: %1").arg(QString::fromUtf8(error.what()));
    update_resource_status();
    if (mode == OpenMode::kInteractive)
      QMessageBox::warning(this, tr("Settings"), application_settings_warning_);
    return false;
  }
}

bool MainWindow::import_application_settings(const QString& path, OpenMode mode) {
  try {
    return apply_application_settings(read_application_settings_file(path, application_settings_), mode);
  } catch (const std::exception& error) {
    application_settings_warning_ = tr("Could not import settings: %1").arg(QString::fromUtf8(error.what()));
    update_resource_status();
    if (mode == OpenMode::kInteractive)
      QMessageBox::warning(this, tr("Settings"), application_settings_warning_);
    return false;
  }
}

bool MainWindow::save_application_settings(const QString& path, OpenMode mode) {
  QString destination = path.isEmpty() ? application_settings_path_ : absolute_document_path(path);
  if (destination.isEmpty() && mode == OpenMode::kInteractive)
    destination = QFileDialog::getSaveFileName(this, tr("Save Settings"), {}, tr("JWP settings (*.cfg);;All files (*)"));
  if (destination.isEmpty()) return false;
  try {
    if (find_document_path(destination) >= 0)
      throw core::JwpConfigurationError("Close the settings document before overwriting it");
    auto next = read_application_settings(write_application_settings(application_settings_));
    write_application_settings_file(destination, next);
    application_settings_ = std::move(next);
    application_settings_path_ = destination;
    application_settings_persistence_enabled_ = true;
    application_settings_warning_.clear();
    update_resource_status();
    return true;
  } catch (const std::exception& error) {
    application_settings_warning_ = tr("Could not save settings: %1").arg(QString::fromUtf8(error.what()));
    update_resource_status();
    if (mode == OpenMode::kInteractive)
      QMessageBox::warning(this, tr("Settings"), application_settings_warning_);
    return false;
  }
}

void MainWindow::configure_application_settings() {
  ApplicationSettingsDialog dialog(application_settings_, this);
  if (dialog.exec() == QDialog::Accepted)
    apply_application_settings(dialog.settings(), OpenMode::kInteractive);
}

bool MainWindow::load_recent_file_configuration(const QString& path, OpenMode mode) {
  recent_files_path_ = absolute_document_path(path);
  try {
    auto files = read_recent_documents(recent_files_path_);
    recent_documents_ = std::move(files);
    recent_file_persistence_enabled_ = true;
    recent_file_warning_.clear();
    update_recent_file_actions();
    update_resource_status();
    return true;
  } catch (const std::exception& error) {
    recent_file_persistence_enabled_ = false;
    recent_file_warning_ = tr("Could not load recent files: %1")
                               .arg(QString::fromUtf8(error.what()));
    update_recent_file_actions();
    update_resource_status();
    if (mode == OpenMode::kInteractive)
      QMessageBox::warning(this, tr("Recent files"), recent_file_warning_);
    return false;
  }
}

bool MainWindow::open_recent_document(int index, OpenMode mode) {
  if (index < 0 || static_cast<std::size_t>(index) >= recent_documents_.size())
    return false;
  const RecentDocument entry = recent_documents_[index];
  if (entry.project) return mode == OpenMode::kInteractive ? open_project_dialog(entry.path)
                                                          : open_project_path(entry.path);
  return entry.encoding ? open_path(entry.path, *entry.encoding, mode, true)
                        : open_jwp_path(entry.path, entry.code_page, mode, true);
}

bool MainWindow::clear_recent_documents(OpenMode mode) {
  try {
    if (!recent_files_path_.isEmpty()) {
      if (find_document_path(recent_files_path_) >= 0)
        throw RecentFilesError("Close the recent history document before clearing it");
      write_recent_documents(recent_files_path_, {});
    }
    recent_documents_.clear();
    recent_file_warning_.clear();
    recent_file_persistence_enabled_ = true;
    update_recent_file_actions();
    update_resource_status();
    return true;
  } catch (const std::exception& error) {
    recent_file_warning_ = tr("Could not clear recent files: %1")
                               .arg(QString::fromUtf8(error.what()));
    update_recent_file_actions();
    update_resource_status();
    if (mode == OpenMode::kInteractive)
      QMessageBox::warning(this, tr("Recent files"), recent_file_warning_);
    return false;
  }
}

void MainWindow::record_recent_document(const DocumentState& state) {
  if (state.current_path_.isEmpty()) return;
  const auto encoding = state.jwp_format_ ? std::nullopt : std::optional{
      state.saved_text_file_ ? state.saved_text_file_->encoding : state.encoding_};
  record_recent_file({state.current_path_, encoding, state.jwp_code_page_});
}

void MainWindow::record_recent_file(RecentDocument entry) {
  try {
    entry.path = absolute_document_path(entry.path);
    auto next = recent_documents_;
    const QString identity = document_path_identity(entry.path);
    next.erase(std::remove_if(next.begin(), next.end(), [&](const auto& old) {
      return document_path_identity(old.path) == identity;
    }), next.end());
    next.insert(next.begin(), std::move(entry));
    if (next.size() > kMaximumRecentDocuments) next.resize(kMaximumRecentDocuments);
    recent_documents_ = std::move(next);
    if (!recent_files_path_.isEmpty() && recent_file_persistence_enabled_ &&
        application_settings_.save_recent_files) {
      if (find_document_path(recent_files_path_) >= 0)
        throw RecentFilesError("History is open as a document; automatic history saving is paused");
      (void)read_recent_documents(recent_files_path_);
      write_recent_documents(recent_files_path_, recent_documents_);
      recent_file_warning_.clear();
    }
  } catch (const std::exception& error) {
    // History is auxiliary: never report completed document I/O as a failure.
    recent_file_warning_ = tr("Could not save recent files: %1")
                               .arg(QString::fromUtf8(error.what()));
  }
  update_recent_file_actions();
  update_resource_status();
  if (!recent_file_warning_.isEmpty()) statusBar()->showMessage(recent_file_warning_, 10000);
}

void MainWindow::update_recent_file_actions() {
  for (qsizetype i = 0; i < recent_file_actions_.size(); ++i) {
    auto* action = recent_file_actions_[i];
    const bool available = static_cast<std::size_t>(i) < recent_documents_.size();
    action->setVisible(available);
    if (!available) continue;
    const auto& entry = recent_documents_[i];
    QString display = entry.path;
    for (qsizetype j = 0; j < display.size(); ++j)
      if (display[j].category() == QChar::Other_Control) display[j] = QLatin1Char(' ');
    display = menuBar()->fontMetrics().elidedText(display, Qt::ElideMiddle, 600);
    action->setText(tr("&%1 %2").arg(i + 1)
                        .arg(display.replace(QLatin1Char('&'), QStringLiteral("&&"))));
    action->setToolTip(entry.path + QStringLiteral("\n") +
                      (entry.project ? tr("JWP project") : entry.encoding ? encoding_name(*entry.encoding)
                                      : tr("JWP (%1)").arg(code_page_name(entry.code_page))));
  }
  if (clear_recent_files_action_)
    clear_recent_files_action_->setEnabled(!recent_documents_.empty() ||
                                           !recent_file_warning_.isEmpty());
}

MainWindow::~MainWindow() {
  document_tabs_->disconnect(this);
  conversion_candidates_->disconnect(this);
  for (const auto& state : documents_) {
    state->editor_->disconnect(this);
    state->editor_->document()->disconnect(this);
    state->editor_->removeEventFilter(this);
    state->editor_->viewport()->removeEventFilter(this);
  }
  delete wnn_user_dictionary_dialog_;
  delete edict_lookup_dialog_;
  delete edict_results_window_;
  delete edict_user_dictionary_dialog_;
  for (auto* dialog : findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog"),
                                            Qt::FindDirectChildrenOnly)) delete dialog;
  delete jis_table_dialog_;
  delete kanji_count_dialog_;
  delete kanji_code_lookup_dialog_;
  delete kanji_reading_lookup_dialog_;
  delete kanji_lookup_dialog_;
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::StyleChange) {
    update_command_bar_palette();
  }
}

void MainWindow::update_command_bar_palette() {
  // Read the window palette, not the menu's previous stylesheet overrides.
  const QPalette source = palette();
  const auto luminance = [](QColor color) {
    const auto linear = [](double channel) {
      return channel <= 0.04045 ? channel / 12.92
                               : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
           0.0722 * linear(color.blueF());
  };
  const auto readable = [&](QColor preferred, QColor background,
                            QPalette::ColorGroup group, double minimum) {
    const double back = luminance(background);
    const auto contrast = [&](QColor color) {
      const double front = luminance(color);
      return (std::max(front, back) + 0.05) / (std::min(front, back) + 0.05);
    };
    for (QColor color : {preferred, source.color(group, QPalette::Text)}) {
      color.setAlpha(255);
      if (contrast(color) >= minimum) return color;
    }
    return contrast(Qt::black) >= contrast(Qt::white) ? QColor(Qt::black)
                                                     : QColor(Qt::white);
  };
  QColor background = source.color(QPalette::Window);
  QColor highlight = source.color(QPalette::Highlight);
  background.setAlpha(255);
  highlight.setAlpha(255);
  const QColor text = readable(source.color(QPalette::WindowText), background,
                              QPalette::Active, 4.5);
  const QColor selected = readable(source.color(QPalette::HighlightedText),
                                  highlight, QPalette::Active, 4.5);
  const QColor disabled = readable(
      source.color(QPalette::Disabled, QPalette::WindowText), background,
      QPalette::Disabled, 3.0);
  const QString rules = QStringLiteral(
      "QMenuBar { background-color: %1; color: %2; }"
      "QMenuBar::item { background-color: transparent; color: %2; }"
      "QMenuBar::item:selected { background-color: %3; color: %4; }"
      "QMenuBar::item:disabled { color: %5; }")
      .arg(background.name(), text.name(), highlight.name(), selected.name(),
           disabled.name());
  if (menuBar()->styleSheet() != rules) menuBar()->setStyleSheet(rules);

  if (main_toolbar_ == nullptr) return;
  QPalette toolbar_palette = source;
  for (const auto group : {QPalette::Active, QPalette::Inactive,
                           QPalette::Disabled}) {
    const QColor foreground = readable(
        source.color(group, QPalette::ButtonText), background, group,
        group == QPalette::Disabled ? 3.0 : 4.5);
    toolbar_palette.setColor(group, QPalette::ButtonText, foreground);
    toolbar_palette.setColor(group, QPalette::WindowText, foreground);
  }
  main_toolbar_->setPalette(toolbar_palette);
  for (const auto& [action, original] : toolbar_standard_icons_) {
    if (original.isNull()) continue;
    QIcon icon;
    for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected, QIcon::Disabled}) {
      const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
      for (const auto state : {QIcon::Off, QIcon::On}) {
        for (const int size : {16, 32, 64}) {
          QImage image = original.pixmap(QSize(size, size), 1.0, mode, state)
                             .toImage().convertToFormat(QImage::Format_ARGB32);
          int opaque = 0;
          int visible = 0;
          bool grayscale = true;
          const QImage normal = original.pixmap(QSize(size, size), 1.0, QIcon::Normal, state).toImage();
          for (int y = 0; y < normal.height(); ++y) {
            for (int x = 0; x < normal.width(); ++x) {
              const QColor pixel = normal.pixelColor(x, y);
              if (pixel.alpha() >= 128) {
                grayscale = grayscale &&
                    std::max({pixel.red(), pixel.green(), pixel.blue()}) -
                        std::min({pixel.red(), pixel.green(), pixel.blue()}) <= 24;
              }
            }
          }
          for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
              const QColor pixel = image.pixelColor(x, y);
              if (pixel.alpha() < 128) continue;
              ++opaque;
              const double alpha = pixel.alphaF();
              const QColor blended = QColor::fromRgbF(
                  pixel.redF() * alpha + background.redF() * (1 - alpha),
                  pixel.greenF() * alpha + background.greenF() * (1 - alpha),
                  pixel.blueF() * alpha + background.blueF() * (1 - alpha));
              const double front = luminance(blended), back = luminance(background);
              if ((std::max(front, back) + 0.05) / (std::min(front, back) + 0.05) >= 3.0) {
                ++visible;
              }
            }
          }
          // Repair unreadable monochrome themes without flattening colorful artwork.
          if (grayscale && opaque != 0 && visible * 8 < opaque) {
            const QColor foreground = toolbar_palette.color(group, QPalette::ButtonText);
            for (int y = 0; y < image.height(); ++y) {
              for (int x = 0; x < image.width(); ++x) {
                QColor color = foreground;
                color.setAlpha(image.pixelColor(x, y).alpha());
                image.setPixelColor(x, y, color);
              }
            }
          }
          icon.addPixmap(QPixmap::fromImage(image), mode, state);
        }
      }
    }
    action->setIcon(icon);
  }
  static const QImage artwork(QStringLiteral(":/jwpqt/toolbar.bmp"));
  if (artwork.size() != QSize(23 * 16, 16)) {
    throw std::runtime_error("Embedded toolbar artwork is invalid");
  }
  for (const auto& [action, index] : toolbar_icons_) {
    QIcon icon;
    for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
      const auto group = mode == QIcon::Disabled ? QPalette::Disabled
                                                : QPalette::Active;
      QImage image = artwork.copy(index * 16, 0, 16, 16)
                        .convertToFormat(QImage::Format_ARGB32);
      for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
          const QColor pixel = image.pixelColor(x, y);
          // Replace the Win32 mask and monochrome ink, retaining colored marks.
          if (pixel == QColor(192, 192, 192)) {
            image.setPixelColor(x, y, Qt::transparent);
          } else if (pixel == QColor(Qt::black)) {
            image.setPixelColor(x, y,
                toolbar_palette.color(group, QPalette::ButtonText));
          } else if (pixel == QColor(Qt::white)) {
            image.setPixelColor(x, y, background);
          }
        }
      }
      icon.addPixmap(QPixmap::fromImage(image), mode);
      icon.addPixmap(QPixmap::fromImage(image.scaled(32, 32)), mode);
    }
    action->setIcon(icon);
  }
}

bool MainWindow::load_kanji_info(const QString& path, OpenMode mode) {
  try {
    std::optional<core::KanjiInfoDatabase> loaded =
        read_kanji_info_file(path);
    std::unique_ptr<core::KanjiInfoDatabase> candidate;
    if (loaded.has_value()) {
      candidate = std::make_unique<core::KanjiInfoDatabase>(std::move(*loaded));
    }
    delete kanji_code_lookup_dialog_;
    kanji_code_lookup_dialog_ = nullptr;
    delete kanji_reading_lookup_dialog_;
    kanji_reading_lookup_dialog_ = nullptr;
    delete kanji_lookup_dialog_;
    kanji_lookup_dialog_ = nullptr;
    delete kanji_count_dialog_;
    kanji_count_dialog_ = nullptr;
    for (auto* dialog : findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog"),
                                              Qt::FindDirectChildrenOnly)) delete dialog;
    kanji_info_database_ = std::move(candidate);
    kanji_info_path_ = path;
    update_resource_status();
    update_kanji_info_action();
    update_jis_table_action();
    update_kanji_count_action();
    update_kanji_code_lookup_actions();
    update_kanji_reading_lookup_action();
    update_kanji_lookup_action();
    statusBar()->showMessage(
        kanji_info_database_ != nullptr
            ? tr("Loaded kanji information for %1 characters")
                  .arg(kanji_info_database_->count())
            : tr("Kanji information database is not installed"),
        3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not load kanji information"), error);
    }
    return false;
  }
}

bool MainWindow::load_kanji_lookup(const QString& radical_path,
                                   const QString& stroke_path,
                                   const QString& radical_bitmap_path,
                                   OpenMode mode) {
  try {
    std::optional<core::KanjiLookupLists> radicals =
        read_kanji_lookup_lists_file(radical_path,
                                     core::kRadicalListGroups);
    std::optional<core::KanjiLookupLists> strokes =
        read_kanji_lookup_lists_file(stroke_path, core::kStrokeListGroups);
    if (radicals.has_value() != strokes.has_value()) {
      throw core::KanjiLookupListError(
          "Both radical.dat and stroke.dat are required");
    }
    std::unique_ptr<core::KanjiLookupLists> radical_candidate;
    std::unique_ptr<core::KanjiLookupLists> stroke_candidate;
    QPixmap sheet;
    if (radicals.has_value()) {
      radical_candidate =
          std::make_unique<core::KanjiLookupLists>(std::move(*radicals));
      stroke_candidate =
          std::make_unique<core::KanjiLookupLists>(std::move(*strokes));
      const QFileInfo bitmap(radical_bitmap_path);
      if (bitmap.exists() && !sheet.load(radical_bitmap_path)) {
        throw core::KanjiLookupListError(
            "The radical sprite sheet is invalid");
      }
    }
    delete kanji_lookup_dialog_;
    kanji_lookup_dialog_ = nullptr;
    radical_lists_ = std::move(radical_candidate);
    stroke_lists_ = std::move(stroke_candidate);
    radical_sheet_ = std::move(sheet);
    update_resource_status();
    update_kanji_lookup_action();
    statusBar()->showMessage(
        has_kanji_lookup() ? tr("Loaded radical and stroke lookup data")
                           : tr("Radical lookup data is not installed"),
        3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not load radical lookup data"), error);
    }
    return false;
  }
}

const core::KanjiInfoDatabase* MainWindow::kanji_info_database()
    const noexcept {
  return kanji_info_database_.get();
}

bool MainWindow::has_kanji_lookup() const noexcept {
  return kanji_info_database_ != nullptr && radical_lists_ != nullptr &&
         stroke_lists_ != nullptr;
}

bool MainWindow::load_edict_configuration(const QString& registry_path,
                                          OpenMode mode) {
  try {
    const std::optional<core::EdictRegistry> loaded =
        read_edict_registry_file(registry_path);
    core::EdictRegistry registry = loaded.value_or(core::EdictRegistry{});
    const QString directory = QFileInfo(registry_path).absolutePath();
    const std::size_t user_index = ensure_edict_user_entry(registry);
    const core::EdictRegistryEntry& user_entry = registry.entries[user_index];
    const core::LegacyCodePage user_code_page =
        core::kDefaultLegacyCodePage;
    const QString user_path = resolve_registry_path(
        decode_registry_text(registry, user_entry.path, user_code_page),
        directory);
    const QString user_label =
        decode_registry_text(registry, user_entry.label, user_code_page);
    std::optional<core::EdictUserDictionary> loaded_user =
        read_edict_user_dictionary_file(user_path, user_code_page);
    auto candidate_user = std::make_unique<EdictUserResources>(
        EdictUserResources{loaded_user ? std::move(*loaded_user)
                                       : core::EdictUserDictionary{},
                           user_path, user_label, user_index, user_code_page});
    auto candidate = std::make_unique<EdictResourceSet>(
        load_edict_resources(registry, directory));

    delete edict_lookup_dialog_;
    edict_lookup_dialog_ = nullptr;
    delete edict_results_window_;
    edict_results_window_ = nullptr;
    delete edict_user_dictionary_dialog_;
    edict_user_dictionary_dialog_ = nullptr;
    edict_resources_ = std::move(candidate);
    edict_user_resources_ = std::move(candidate_user);
    edict_config_directory_ = directory;
    update_resource_status();
    update_edict_actions();
    statusBar()->showMessage(
        tr("Loaded %1 dictionary resources")
            .arg(static_cast<qulonglong>(edict_resources_->resources.size())),
        3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not load dictionary configuration"), error);
    }
    return false;
  }
}

const EdictResourceSet* MainWindow::edict_resources() const noexcept {
  return edict_resources_.get();
}

const core::EdictUserDictionary* MainWindow::edict_user_dictionary()
    const noexcept {
  return edict_user_resources_ == nullptr
             ? nullptr
             : &edict_user_resources_->dictionary;
}

bool MainWindow::set_edict_user_dictionary(
    core::EdictUserDictionary dictionary, OpenMode mode) {
  if (conversion_active()) {
    if (mode == OpenMode::kInteractive) {
      statusBar()->showMessage(
          tr("Accept the current conversion before changing user entries"),
          5000);
    }
    return false;
  }
  finish_kana_input();
  if (conversion_active()) {
    return false;
  }

  try {
    if (edict_resources_ == nullptr || edict_user_resources_ == nullptr) {
      throw std::runtime_error("EDICT user dictionary is not configured");
    }
    const std::size_t registry_index =
        edict_user_resources_->registry_index;
    if (registry_index >= edict_resources_->registry.entries.size()) {
      throw std::runtime_error("EDICT user dictionary registry is stale");
    }
    const core::EdictRegistryEntry& entry =
        edict_resources_->registry.entries[registry_index];
    const std::string bytes =
        dictionary.serialize(edict_user_resources_->code_page);
    std::optional<EdictLoadedResource> loaded;
    if (entry.searched) {
      core::EdictDictionary search_dictionary = core::EdictDictionary::parse(
          bytes, core::EdictEncoding::kMixed, core::EdictParseLimits{},
          edict_user_resources_->code_page);
      loaded.emplace(EdictLoadedResource{
          registry_index,
          entry,
          edict_user_resources_->label,
          edict_user_resources_->path,
          std::nullopt,
          std::move(search_dictionary),
          std::nullopt,
      });
    }
    auto candidate_user = std::make_unique<EdictUserResources>(
        EdictUserResources{std::move(dictionary),
                           edict_user_resources_->path,
                           edict_user_resources_->label,
                           registry_index,
                           edict_user_resources_->code_page});

    auto& resources = edict_resources_->resources;
    auto existing = std::find_if(
        resources.begin(), resources.end(), [registry_index](const auto& item) {
          return item.registry_index == registry_index;
        });
    const bool had_existing = existing != resources.end();
    const std::size_t existing_position =
        static_cast<std::size_t>(std::distance(resources.begin(), existing));
    if (!had_existing && loaded.has_value()) {
      resources.reserve(resources.size() + 1);
    }
    write_edict_user_dictionary_file(candidate_user->path,
                                     candidate_user->dictionary,
                                     candidate_user->code_page);
    if (had_existing && loaded.has_value()) {
      resources[existing_position] = std::move(*loaded);
    } else if (had_existing) {
      resources.erase(resources.begin() +
                      static_cast<std::ptrdiff_t>(existing_position));
    } else if (loaded.has_value()) {
      resources.push_back(std::move(*loaded));
      std::rotate(std::lower_bound(
                      resources.begin(), resources.end() - 1, registry_index,
                      [](const EdictLoadedResource& resource,
                         std::size_t index) {
                        return resource.registry_index < index;
                      }),
                  resources.end() - 1, resources.end());
    }
    auto& failures = edict_resources_->failures;
    failures.erase(
        std::remove_if(failures.begin(), failures.end(),
                       [registry_index](const EdictResourceFailure& failure) {
                         return failure.registry_index == registry_index;
                       }),
        failures.end());
    edict_user_resources_ = std::move(candidate_user);
    update_resource_status();
    update_edict_actions();
    statusBar()->showMessage(tr("Updated user dictionary"), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not update user dictionary"), error);
    }
    return false;
  }
}

bool MainWindow::load_kanji_color_configuration(const QString& settings_path,
                                                const QString& list_path,
                                                OpenMode mode) {
  try {
    QSettings settings(settings_path, QSettings::IniFormat);
    const core::KanjiColorPolicy policy = read_kanji_color_policy(settings);
    const std::optional<core::KanjiColorList> loaded_list =
        read_kanji_color_list_file(list_path);
    const core::KanjiColorList list =
        loaded_list.value_or(core::KanjiColorList{});
    QString retained_settings_path = settings_path;
    QString retained_list_path = list_path;

    if (document_->jwp_document_.has_value()) {
      document_->editor_->apply_kanji_colors(document_->jwp_document_->document(), list, policy,
                                  document_->jwp_code_page_);
    } else {
      document_->editor_->clear_kanji_colors();
    }
    kanji_color_policy_ = policy;
    kanji_color_list_ = list;
    kanji_color_settings_path_.swap(retained_settings_path);
    kanji_color_list_path_.swap(retained_list_path);
    update_kanji_color_actions();
    statusBar()->showMessage(tr("Loaded kanji color configuration"), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not load kanji color configuration"), error);
    }
    return false;
  }
}

const core::KanjiColorPolicy& MainWindow::kanji_color_policy() const noexcept {
  return kanji_color_policy_;
}

const core::KanjiColorList& MainWindow::kanji_color_list() const noexcept {
  return kanji_color_list_;
}

bool MainWindow::set_kanji_color_policy(const core::KanjiColorPolicy& policy,
                                        OpenMode mode) {
  try {
    switch (policy.list_mode) {
      case core::KanjiListColorMode::kOff:
      case core::KanjiListColorMode::kMatch:
      case core::KanjiListColorMode::kNoMatch:
        break;
      default:
        throw std::invalid_argument("Invalid kanji list color mode");
    }
    if (kanji_color_settings_path_.isEmpty()) {
      throw std::runtime_error("Kanji color settings path is not configured");
    }

    if (document_->jwp_document_.has_value()) {
      document_->editor_->apply_kanji_colors(document_->jwp_document_->document(),
                                  kanji_color_list_, policy, document_->jwp_code_page_);
    }
    try {
      QSettings settings(kanji_color_settings_path_, QSettings::IniFormat);
      write_kanji_color_policy(settings, policy);
    } catch (...) {
      if (document_->jwp_document_.has_value()) {
        document_->editor_->apply_kanji_colors(document_->jwp_document_->document(),
                                    kanji_color_list_, kanji_color_policy_,
                                    document_->jwp_code_page_);
      }
      throw;
    }

    kanji_color_policy_ = policy;
    statusBar()->showMessage(tr("Updated kanji color options"), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not update kanji color options"), error);
    }
    return false;
  }
}

bool MainWindow::set_kanji_color_list(core::KanjiColorList color_list,
                                      OpenMode mode) {
  if (conversion_active()) {
    if (mode == OpenMode::kInteractive) {
      statusBar()->showMessage(
          tr("Accept the current conversion before changing the kanji list"),
          5000);
    }
    return false;
  }
  try {
    if (kanji_color_list_path_.isEmpty()) {
      throw std::runtime_error("Kanji color list path is not configured");
    }

    QList<QTextEdit::ExtraSelection> prepared_colors;
    if (document_->jwp_document_.has_value()) {
      prepared_colors = document_->editor_->prepare_kanji_colors(
          document_->jwp_document_->document(), color_list, kanji_color_policy_,
          document_->jwp_code_page_);
    }
    write_kanji_color_list_file(kanji_color_list_path_, color_list);
    kanji_color_list_ = std::move(color_list);
    if (document_->jwp_document_.has_value()) {
      document_->editor_->set_kanji_color_selections(std::move(prepared_colors));
    }
    update_kanji_color_actions();
    statusBar()->showMessage(tr("Updated kanji color list"), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not update kanji color list"), error);
    }
    return false;
  }
}

bool MainWindow::make_kanji_color_list(OpenMode mode) {
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return false;
  }
  core::KanjiColorList color_list;
  color_list.add_document(document_->jwp_document_->document());
  return set_kanji_color_list(std::move(color_list), mode);
}

bool MainWindow::append_kanji_color_list(OpenMode mode) {
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return false;
  }
  core::KanjiColorList color_list = kanji_color_list_;
  color_list.add_document(document_->jwp_document_->document());
  return set_kanji_color_list(std::move(color_list), mode);
}

bool MainWindow::edit_kanji_color_list(const QString& text, bool add,
                                       OpenMode mode) {
  if (conversion_active()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active()) {
    return false;
  }
  try {
    const core::JwpText encoded =
        core::encode_jwp_text(from_qstring(text), document_->jwp_code_page_);
    core::KanjiColorList color_list = kanji_color_list_;
    for (const core::JisCode code : encoded) {
      if (add) {
        color_list.add(code);
      } else {
        color_list.remove(code);
      }
    }
    return set_kanji_color_list(std::move(color_list), mode);
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not edit kanji color list"), error);
    }
    return false;
  }
}

bool MainWindow::view_kanji_color_list(OpenMode mode) {
  if (conversion_active()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active()) {
    return false;
  }
  try {
    core::JwpDocument document;
    document.paragraphs.emplace_back();
    document.paragraphs.front().text = kanji_color_list_.codes();
    load_jwp_document(QString(), std::move(document), document_->jwp_code_page_,
                      true);
    statusBar()->showMessage(tr("Viewing kanji color list"), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not view kanji color list"), error);
    }
    return false;
  }
}

bool MainWindow::clear_kanji_color_list(OpenMode mode) {
  if (conversion_active()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active()) {
    return false;
  }
  return set_kanji_color_list(core::KanjiColorList{}, mode);
}

void MainWindow::create_actions() {
  QMenu* file_menu = menuBar()->addMenu(tr("&File"));

  QAction* new_action = file_menu->addAction(tr("&New Japanese Document"));
  new_action->setObjectName(QStringLiteral("newDocumentAction"));
  new_action->setShortcut(QKeySequence::New);
  connect(new_action, &QAction::triggered, this,
          [this] { new_document_tab(); });

  QAction* new_text_action = file_menu->addAction(tr("New &Text Document"));
  new_text_action->setObjectName(QStringLiteral("newTextDocumentAction"));
  new_text_action->setStatusTip(
      tr("Create unrestricted Unicode text without JWP formatting"));
  connect(new_text_action, &QAction::triggered, this, [this] {
    new_document_tab(false);
  });

  QAction* open_action = file_menu->addAction(tr("&Open..."));
  open_action->setObjectName(QStringLiteral("openDocumentAction"));
  open_action->setShortcut(QKeySequence::Open);
  connect(open_action, &QAction::triggered, this,
          [this] { open_document(); });

  revert_action_ = file_menu->addAction(tr("&Revert"));
  revert_action_->setObjectName(QStringLiteral("revertDocumentAction"));
  revert_action_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_R));
  connect(revert_action_, &QAction::triggered, this,
          [this] { (void)revert_current_document(); });

  QAction* close_action = file_menu->addAction(tr("&Close"));
  close_action->setObjectName(QStringLiteral("closeDocumentAction"));
  close_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+F4")));
  connect(close_action, &QAction::triggered, this,
          [this] { close_document(current_document_index()); });
  QAction* close_all_action = file_menu->addAction(tr("Close A&ll"));
  close_all_action->setObjectName(QStringLiteral("closeAllDocumentsAction"));
  connect(close_all_action, &QAction::triggered, this,
          [this] { close_all_documents(); });

  QAction* save_action = file_menu->addAction(tr("&Save"));
  save_action->setObjectName(QStringLiteral("saveDocumentAction"));
  save_action->setShortcut(QKeySequence::Save);
  connect(save_action, &QAction::triggered, this,
          [this] { save_document(); });
  QAction* save_all_action = file_menu->addAction(tr("Save All"));
  save_all_action->setObjectName(QStringLiteral("saveAllDocumentsAction"));
  connect(save_all_action, &QAction::triggered, this,
          [this] { save_all_documents(); });

  QAction* save_as_action = file_menu->addAction(tr("Save &As..."));
  save_as_action->setShortcut(QKeySequence::SaveAs);
  connect(save_as_action, &QAction::triggered, this,
          [this] { save_document_as(); });

  QAction* export_action = file_menu->addAction(tr("Export &Copy..."));
  export_action->setObjectName(QStringLiteral("exportCopyAction"));
  connect(export_action, &QAction::triggered, this,
          [this] { save_document_as(true); });

  delete_action_ = file_menu->addAction(tr("&Delete File"));
  delete_action_->setObjectName(QStringLiteral("deleteDocumentAction"));
  connect(delete_action_, &QAction::triggered, this,
          [this] { (void)delete_current_document(); });

  file_menu->addSeparator();
  auto* open_project_action = file_menu->addAction(tr("Open Project..."));
  open_project_action->setObjectName(QStringLiteral("openProjectAction"));
  connect(open_project_action, &QAction::triggered, this, [this] { open_project_dialog(); });
  auto* save_project_action = file_menu->addAction(tr("Save Project..."));
  save_project_action->setObjectName(QStringLiteral("saveProjectAction"));
  connect(save_project_action, &QAction::triggered, this, &MainWindow::save_project_dialog);
  file_menu->addSeparator();
  print_action_ = file_menu->addAction(tr("&Print..."));
  print_action_->setObjectName(QStringLiteral("printAction"));
  print_action_->setShortcut(QKeySequence::Print);
  connect(print_action_, &QAction::triggered, this,
          [this] { print_current_document(); });

  printer_setup_action_ = file_menu->addAction(tr("Printer Set&up..."));
  printer_setup_action_->setObjectName(QStringLiteral("printerSetupAction"));
  connect(printer_setup_action_, &QAction::triggered, this,
          [this] { setup_printer(); });

  file_menu->addSeparator();
  auto* recent_files_menu = file_menu->addMenu(tr("Recent &Files"));
  recent_files_menu->setObjectName(QStringLiteral("recentFilesMenu"));
  // A triggered action must survive the resulting change in recent-file order.
  for (std::size_t i = 0; i < kMaximumRecentDocuments; ++i) {
    auto* action = recent_files_menu->addAction(QString());
    action->setObjectName(QStringLiteral("recentFile%1Action").arg(i + 1));
    recent_file_actions_.append(action);
    connect(action, &QAction::triggered, this, [this, i] {
      open_recent_document(static_cast<int>(i), OpenMode::kInteractive);
    });
  }
  recent_files_menu->addSeparator();
  clear_recent_files_action_ = recent_files_menu->addAction(tr("Clear Recent Files"));
  clear_recent_files_action_->setObjectName(QStringLiteral("clearRecentFilesAction"));
  connect(clear_recent_files_action_, &QAction::triggered, this,
          [this] { clear_recent_documents(OpenMode::kInteractive); });
  update_recent_file_actions();
  file_menu->addSeparator();
  QAction* quit_action = file_menu->addAction(tr("&Quit"));
  quit_action->setShortcut(QKeySequence::Quit);
  connect(quit_action, &QAction::triggered, this, &QWidget::close);

  QMenu* edit_menu = menuBar()->addMenu(tr("&Edit"));

  undo_action_ = edit_menu->addAction(tr("&Undo"));
  undo_action_->setObjectName(QStringLiteral("undoAction"));
  undo_action_->setShortcut(QKeySequence::Undo);
  undo_action_->setEnabled(false);
  connect(undo_action_, &QAction::triggered, this,
          [this] { undo_document(); });

  redo_action_ = edit_menu->addAction(tr("&Redo"));
  redo_action_->setObjectName(QStringLiteral("redoAction"));
  redo_action_->setShortcut(QKeySequence::Redo);
  redo_action_->setEnabled(false);
  connect(redo_action_, &QAction::triggered, this,
          [this] { redo_document(); });

  edit_menu->addSeparator();
  QAction* cut_action = edit_menu->addAction(tr("Cu&t"));
  cut_action_ = cut_action;
  cut_action->setObjectName(QStringLiteral("cutAction"));
  cut_action->setShortcut(QKeySequence::Cut);
  cut_action->setEnabled(false);
  connect(cut_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    document_->editor_->cut();
  });

  QAction* copy_action = edit_menu->addAction(tr("&Copy"));
  copy_action_ = copy_action;
  copy_action->setObjectName(QStringLiteral("copyAction"));
  copy_action->setShortcuts({QKeySequence::Copy, QKeySequence(QStringLiteral("Ctrl+Insert"))});
  copy_action->setEnabled(false);
  connect(copy_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    document_->editor_->copy();
  });

  QAction* paste_action = edit_menu->addAction(tr("&Paste"));
  paste_action->setObjectName(QStringLiteral("pasteAction"));
  paste_action->setShortcuts({QKeySequence::Paste, QKeySequence(QStringLiteral("Shift+Insert")),
                             QKeySequence(QStringLiteral("Ctrl+Shift+Insert"))});
  connect(paste_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    document_->editor_->paste();
  });

  edit_menu->addSeparator();
  QAction* select_all_action = edit_menu->addAction(tr("Select &All"));
  select_all_action->setShortcut(QKeySequence::SelectAll);
  connect(select_all_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    document_->editor_->selectAll();
  });

  // The standard QTextEdit menu would bypass portable JWP history.
  editor_actions_ = {undo_action_, redo_action_, cut_action, copy_action,
                     paste_action, select_all_action};

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

  QMenu* input_menu = edit_menu->addMenu(tr("Input &Mode"));
  const auto add_input_mode = [&](InputMode mode, const QString& title,
                                  const char* name, const QString& shortcut) {
    QAction* action = input_menu->addAction(title);
    action->setObjectName(QString::fromLatin1(name));
    action->setCheckable(true);
    action->setData(static_cast<int>(mode));
    action->setShortcut(QKeySequence(shortcut));
    input_mode_actions_->addAction(action);
    return action;
  };
  kana_input_action_ = add_input_mode(InputMode::kKanji, tr("&Kanji"),
                                      "kanaInputAction", QStringLiteral("Ctrl+K"));
  kana_input_action_->setStatusTip(tr("Compose hiragana and katakana from romaji"));
  QAction* ascii_input = add_input_mode(
      InputMode::kAscii, tr("&ASCII"), "asciiInputAction", QStringLiteral("Ctrl+Alt+A"));
  QAction* jascii_input = add_input_mode(
      InputMode::kJascii, tr("&JASCII"), "jasciiInputAction", QStringLiteral("Ctrl+J"));
  connect(input_mode_actions_, &QActionGroup::triggered, this,
          [this](QAction* action) {
            set_input_mode(static_cast<InputMode>(action->data().toInt()));
          });
  input_menu->addSeparator();
  toggle_input_mode_action_ = input_menu->addAction(tr("Switch Kanji/ASCII"));
  toggle_input_mode_action_->setObjectName(QStringLiteral("toggleInputModeAction"));
  toggle_input_mode_action_->setShortcut(QKeySequence(Qt::Key_F4));
  connect(toggle_input_mode_action_, &QAction::triggered, this, [this] {
    set_input_mode(document_->input_mode_ == InputMode::kKanji ? InputMode::kAscii
                                                   : InputMode::kKanji);
  });
  input_menu->addSeparator();
  japanese_editing_action_ = input_menu->addAction(tr("Japanese &Editing"));
  japanese_editing_action_->setObjectName(QStringLiteral("japaneseEditingAction"));
  japanese_editing_action_->setCheckable(true);
  japanese_editing_action_->setStatusTip(
      tr("Disable for unrestricted Unicode; the saved file format stays unchanged"));
  connect(japanese_editing_action_, &QAction::triggered, this, [this](bool enabled) {
    japanese_editing_action_->setChecked(is_jwp_document());
    if (QMessageBox::warning(
            this, tr("Change editing mode"),
            tr("Changing editing mode clears Undo/Redo. Unicode editing also "
               "removes JWP layout, hard page breaks and document metadata. "
               "The saved file format stays unchanged. Continue?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) ==
        QMessageBox::Yes) {
      set_japanese_editing(enabled, true, OpenMode::kInteractive);
    }
  });

  input_menu->addSeparator();
  overwrite_action_ = input_menu->addAction(tr("&Overwrite Mode"));
  overwrite_action_->setObjectName(QStringLiteral("overwriteModeAction"));
  overwrite_action_->setCheckable(true);
  overwrite_action_->setShortcut(QKeySequence(Qt::Key_Insert));
  connect(overwrite_action_, &QAction::toggled, this, [this](bool overwrite) {
    for (const auto& state : documents_) state->editor_->setOverwriteMode(overwrite);
    overwrite_button_->setChecked(overwrite);
    overwrite_button_->setText(overwrite ? tr("OVR") : tr("INS"));
    overwrite_button_->setToolTip(overwrite
        ? tr("Overwrite: click or press Insert to switch to insertion")
        : tr("Insert: click or press Insert to toggle overwrite"));
  });

  QMenu* format_menu = menuBar()->addMenu(tr("F&ormat"));
  format_file_action_ = format_menu->addAction(tr("Format &File..."));
  format_file_action_->setObjectName(QStringLiteral("formatFileAction"));
  format_file_action_->setShortcut(
      QKeySequence(QStringLiteral("Alt+Ctrl+F")));
  connect(format_file_action_, &QAction::triggered, this,
          [this] { format_file_paragraphs(); });

  format_paragraph_action_ =
      format_menu->addAction(tr("&Paragraph..."));
  format_paragraph_action_->setObjectName(
      QStringLiteral("formatParagraphAction"));
  format_paragraph_action_->setShortcut(
      QKeySequence(QStringLiteral("Alt+Shift+F")));
  connect(format_paragraph_action_, &QAction::triggered, this,
          [this] { format_document_paragraphs(); });

  page_layout_action_ = format_menu->addAction(tr("Page &Layout..."));
  page_layout_action_->setObjectName(QStringLiteral("pageLayoutAction"));
  page_layout_action_->setShortcut(QKeySequence(QStringLiteral("Alt+L")));
  connect(page_layout_action_, &QAction::triggered, this,
          [this] { format_page_layout(); });

  insert_page_break_action_ =
      format_menu->addAction(tr("Insert Page &Break"));
  insert_page_break_action_->setObjectName(
      QStringLiteral("insertPageBreakAction"));
  insert_page_break_action_->setShortcuts(
      {QKeySequence(Qt::CTRL | Qt::Key_Return),
       QKeySequence(Qt::CTRL | Qt::Key_Enter)});
  connect(insert_page_break_action_, &QAction::triggered, this,
          [this] { insert_page_break(); });

  QMenu* tools_menu = menuBar()->addMenu(tr("&Tools"));
  auto* options_action = tools_menu->addAction(tr("&Options..."));
  options_action->setObjectName(QStringLiteral("applicationOptionsAction"));
  connect(options_action, &QAction::triggered, this, &MainWindow::configure_application_settings);
  auto* defaults_action = tools_menu->addAction(tr("Default Settings..."));
  defaults_action->setObjectName(QStringLiteral("defaultSettingsAction"));
  connect(defaults_action, &QAction::triggered, this, [this] {
    if (QMessageBox::question(this, tr("Default Settings"),
        tr("Reset native options to defaults? Retained legacy-only settings will be kept."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    ApplicationSettings defaults;
    defaults.source = application_settings_.source;
    apply_application_settings(defaults, OpenMode::kInteractive);
  });
  auto* save_settings_action = tools_menu->addAction(tr("Save Settings"));
  save_settings_action->setObjectName(QStringLiteral("saveSettingsAction"));
  connect(save_settings_action, &QAction::triggered, this,
          [this] { save_application_settings({}, OpenMode::kInteractive); });
  auto* import_settings_action = tools_menu->addAction(tr("Import Settings..."));
  import_settings_action->setObjectName(QStringLiteral("importSettingsAction"));
  connect(import_settings_action, &QAction::triggered, this, [this] {
    const auto path = QFileDialog::getOpenFileName(this, tr("Import Settings"), {},
        tr("JWP settings (*.cfg);;All files (*)"));
    if (!path.isEmpty()) import_application_settings(path, OpenMode::kInteractive);
  });
  tools_menu->addSeparator();
  edict_lookup_action_ =
      tools_menu->addAction(tr("&Dictionary Lookup..."));
  edict_lookup_action_->setObjectName(QStringLiteral("edictLookupAction"));
  edict_lookup_action_->setShortcuts(
      {QKeySequence(QStringLiteral("Ctrl+D")), QKeySequence(Qt::Key_F6)});
  connect(edict_lookup_action_, &QAction::triggered, this,
          [this] { show_edict_lookup_dialog(); });

  edict_results_action_ = tools_menu->addAction(tr("Dictionary &Results"));
  edict_results_action_->setObjectName(QStringLiteral("edictResultsAction"));
  edict_results_action_->setEnabled(false);
  connect(edict_results_action_, &QAction::triggered, this, [this] {
    show_edict_results_window();
  });

  edict_user_dictionary_action_ =
      tools_menu->addAction(tr("&User Dictionary..."));
  edict_user_dictionary_action_->setObjectName(
      QStringLiteral("edictUserDictionaryAction"));
  connect(edict_user_dictionary_action_, &QAction::triggered, this,
          [this] { show_edict_user_dictionary_dialog(); });

  kanji_info_action_ = tools_menu->addAction(tr("Character &Information"));
  kanji_info_action_->setObjectName(QStringLiteral("kanjiInfoAction"));
  kanji_info_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
  connect(kanji_info_action_, &QAction::triggered, this,
          [this] { show_kanji_info_dialog(); });

  auto* info_setup = tools_menu->addAction(tr("Character Info &Setup..."));
  info_setup->setObjectName(QStringLiteral("kanjiInfoSetupAction"));
  connect(info_setup, &QAction::triggered, this, [this] {
    KanjiInfoOptionsDialog dialog(application_settings_.kanji_info, this);
    if (dialog.exec() != QDialog::Accepted) return;
    auto next = application_settings_;
    next.kanji_info = dialog.options();
    apply_application_settings(next, OpenMode::kInteractive);
  });

  jis_table_action_ = tools_menu->addAction(tr("&JIS Table"));
  jis_table_action_->setObjectName(QStringLiteral("jisTableAction"));
  jis_table_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
  connect(jis_table_action_, &QAction::triggered, this,
          [this] { show_jis_table_dialog(); });

  skip_lookup_action_ = tools_menu->addAction(tr("&SKIP Lookup"));
  skip_lookup_action_->setObjectName(QStringLiteral("skipLookupAction"));
  skip_lookup_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Alt+S")));
  connect(skip_lookup_action_, &QAction::triggered, this,
          [this] { show_kanji_code_lookup_dialog(KanjiCodeLookupMode::kSkip); });

  four_corner_lookup_action_ =
      tools_menu->addAction(tr("&Four-Corner Lookup"));
  four_corner_lookup_action_->setObjectName(
      QStringLiteral("fourCornerLookupAction"));
  four_corner_lookup_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+4")));
  connect(four_corner_lookup_action_, &QAction::triggered, this,
          [this] {
            show_kanji_code_lookup_dialog(KanjiCodeLookupMode::kFourCorner);
          });

  bushu_lookup_action_ = tools_menu->addAction(tr("&Bushu Lookup"));
  bushu_lookup_action_->setObjectName(QStringLiteral("bushuLookupAction"));
  bushu_lookup_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+L")));
  connect(bushu_lookup_action_, &QAction::triggered, this,
           [this] { show_kanji_code_lookup_dialog(KanjiCodeLookupMode::kBushu); });

  stroke_bushu_lookup_action_ =
      tools_menu->addAction(tr("Str&oke/Bushu Lookup"));
  stroke_bushu_lookup_action_->setObjectName(
      QStringLiteral("strokeBushuLookupAction"));
  stroke_bushu_lookup_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+B")));
  connect(stroke_bushu_lookup_action_, &QAction::triggered, this, [this] {
    show_kanji_code_lookup_dialog(KanjiCodeLookupMode::kStrokeBushu);
  });

  spahn_lookup_action_ =
      tools_menu->addAction(tr("Spahn-&Hadamitzky Lookup"));
  spahn_lookup_action_->setObjectName(QStringLiteral("spahnLookupAction"));
  spahn_lookup_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+H")));
  connect(spahn_lookup_action_, &QAction::triggered, this,
          [this] { show_kanji_code_lookup_dialog(KanjiCodeLookupMode::kSpahn); });

  kanji_reading_lookup_action_ =
      tools_menu->addAction(tr("&Reading Lookup"));
  kanji_reading_lookup_action_->setObjectName(
      QStringLiteral("kanjiReadingLookupAction"));
  kanji_reading_lookup_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+R")));
  connect(kanji_reading_lookup_action_, &QAction::triggered, this,
          [this] { show_kanji_reading_lookup_dialog(); });

  index_lookup_action_ = tools_menu->addAction(tr("&Index Lookup"));
  index_lookup_action_->setObjectName(QStringLiteral("indexLookupAction"));
  index_lookup_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
  connect(index_lookup_action_, &QAction::triggered, this,
          [this] { show_kanji_code_lookup_dialog(KanjiCodeLookupMode::kIndex); });

  kanji_count_action_ = tools_menu->addAction(tr("&Count Kanji"));
  kanji_count_action_->setObjectName(QStringLiteral("kanjiCountAction"));
  kanji_count_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+K")));
  connect(kanji_count_action_, &QAction::triggered, this,
          [this] { show_kanji_count_dialog(); });

  kanji_lookup_action_ = tools_menu->addAction(tr("&Radical Lookup"));
  kanji_lookup_action_->setObjectName(QStringLiteral("radicalLookupAction"));
  kanji_lookup_action_->setShortcut(QKeySequence(Qt::Key_F5));
  connect(kanji_lookup_action_, &QAction::triggered, this,
          [this] { show_kanji_lookup_dialog(); });

  tools_menu->addSeparator();
  kanji_color_options_action_ =
      tools_menu->addAction(tr("Kanji Color &Options..."));
  kanji_color_options_action_->setObjectName(
      QStringLiteral("kanjiColorOptionsAction"));
  connect(kanji_color_options_action_, &QAction::triggered, this,
          [this] { configure_kanji_colors(); });

  tools_menu->addSeparator();
  make_kanji_color_list_action_ =
      tools_menu->addAction(tr("&Make Kanji Color List"));
  make_kanji_color_list_action_->setObjectName(
      QStringLiteral("makeKanjiColorListAction"));
  connect(make_kanji_color_list_action_, &QAction::triggered, this,
          [this] { make_kanji_color_list(); });

  append_kanji_color_list_action_ =
      tools_menu->addAction(tr("&Append Document to Kanji Color List"));
  append_kanji_color_list_action_->setObjectName(
      QStringLiteral("appendKanjiColorListAction"));
  connect(append_kanji_color_list_action_, &QAction::triggered, this,
          [this] { append_kanji_color_list(); });

  edit_kanji_color_list_action_ =
      tools_menu->addAction(tr("Add or &Remove Kanji..."));
  edit_kanji_color_list_action_->setObjectName(
      QStringLiteral("editKanjiColorListAction"));
  connect(edit_kanji_color_list_action_, &QAction::triggered, this,
          [this] { edit_kanji_color_list(); });

  view_kanji_color_list_action_ =
      tools_menu->addAction(tr("&View Kanji Color List"));
  view_kanji_color_list_action_->setObjectName(
      QStringLiteral("viewKanjiColorListAction"));
  connect(view_kanji_color_list_action_, &QAction::triggered, this,
          [this] { view_kanji_color_list(); });

  clear_kanji_color_list_action_ =
      tools_menu->addAction(tr("&Clear Kanji Color List"));
  clear_kanji_color_list_action_->setObjectName(
      QStringLiteral("clearKanjiColorListAction"));
  connect(clear_kanji_color_list_action_, &QAction::triggered, this,
          [this] { clear_kanji_color_list(); });

  QMenu* convert_menu = menuBar()->addMenu(tr("&Convert"));
  convert_action_ = convert_menu->addAction(tr("Convert &Selection"));
  convert_action_->setObjectName(QStringLiteral("convertSelectionAction"));
  convert_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
  connect(convert_action_, &QAction::triggered, this,
          [this] { convert_selection(); });

  previous_candidate_action_ =
      convert_menu->addAction(tr("&Previous Candidate"));
  previous_candidate_action_->setObjectName(
      QStringLiteral("previousCandidateAction"));
  previous_candidate_action_->setShortcut(
      QKeySequence(Qt::SHIFT | Qt::Key_Space));
  connect(previous_candidate_action_, &QAction::triggered, this,
          [this] { cycle_conversion(true); });

  next_candidate_action_ = convert_menu->addAction(tr("&Next Candidate"));
  next_candidate_action_->setObjectName(QStringLiteral("nextCandidateAction"));
  next_candidate_action_->setShortcut(QKeySequence(Qt::Key_Space));
  connect(next_candidate_action_, &QAction::triggered, this,
          [this] { cycle_conversion(false); });

  accept_candidate_action_ =
      convert_menu->addAction(tr("&Accept Candidate"));
  accept_candidate_action_->setObjectName(
      QStringLiteral("acceptCandidateAction"));
  accept_candidate_action_->setShortcuts(
      {QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter),
       QKeySequence(Qt::Key_Escape)});
  connect(accept_candidate_action_, &QAction::triggered, this,
          [this] { accept_conversion(); });

  convert_menu->addSeparator();
  user_dictionary_action_ =
      convert_menu->addAction(tr("&User Conversions..."));
  user_dictionary_action_->setObjectName(
      QStringLiteral("userDictionaryAction"));
  connect(user_dictionary_action_, &QAction::triggered, this,
          [this] { show_wnn_user_dictionary_dialog(); });

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
            [this, code_page] {
              set_jwp_code_page(code_page);
              if (document_->jwp_code_page_ == code_page)
                application_settings_.translation_code_page = static_cast<int>(code_page);
            });
  }
  QMenu* window_menu = menuBar()->addMenu(tr("&Window"));
  next_file_action_ = window_menu->addAction(tr("&Next File"));
  next_file_action_->setObjectName(QStringLiteral("nextFileAction"));
  next_file_action_->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Tab),
                                  QKeySequence(Qt::CTRL | Qt::Key_PageDown)});
  connect(next_file_action_, &QAction::triggered, this, [this] {
    activate_document((current_document_index() + 1) % document_count());
  });
  previous_file_action_ = window_menu->addAction(tr("&Previous File"));
  previous_file_action_->setObjectName(QStringLiteral("previousFileAction"));
  previous_file_action_->setShortcuts({QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab),
                                      QKeySequence(Qt::CTRL | Qt::Key_PageUp)});
  connect(previous_file_action_, &QAction::triggered, this, [this] {
    activate_document((current_document_index() + document_count() - 1) % document_count());
  });
  QAction* files_action = window_menu->addAction(tr("&Files..."));
  files_action->setObjectName(QStringLiteral("filesAction"));
  files_action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_W));
  connect(files_action, &QAction::triggered, this, [this] {
    QStringList files;
    for (int i = 0; i < document_count(); ++i) {
      const auto& state = *documents_[i];
      const QString name = state.current_path_.isEmpty() ? tr("Untitled") : state.current_path_;
      files << tr("%1. %2").arg(i + 1).arg(name);
    }
    QInputDialog dialog(this);
    dialog.setObjectName(QStringLiteral("filesDialog"));
    dialog.setWindowTitle(tr("Open Files"));
    dialog.setLabelText(tr("Activate a document:"));
    dialog.setComboBoxItems(files);
    dialog.setOption(QInputDialog::UseListViewForComboBoxItems);
    dialog.setTextValue(files[current_document_index()]);
    if (dialog.exec() == QDialog::Accepted)
      activate_document(files.indexOf(dialog.textValue()));
  });

  QMenu* help_menu = menuBar()->addMenu(tr("&Help"));
  QAction* resources = help_menu->addAction(tr("Runtime &Resources..."));
  resources->setObjectName(QStringLiteral("resourceStatusAction"));
  connect(resources, &QAction::triggered, this, [this] {
    QMessageBox dialog(QMessageBox::Information, tr("Runtime Resources"), {},
                       QMessageBox::Ok, this);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setText(resource_report());
    dialog.exec();
  });
  connect(resource_status_button_, &QToolButton::clicked,
          resources, &QAction::trigger);

  main_toolbar_ = addToolBar(tr("Main Toolbar"));
  main_toolbar_->setObjectName(QStringLiteral("mainToolBar"));
  main_toolbar_->setIconSize(QSize(16, 16));
  main_toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  main_toolbar_->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
  main_toolbar_->setFloatable(false);
  const auto add_standard = [&](QAction* action, const char* theme,
                                const QString& label, QIcon fallback = {}) {
    action->setIcon(QIcon::fromTheme(QString::fromLatin1(theme), fallback));
    toolbar_standard_icons_.emplace_back(action, action->icon());
    action->setIconText(label);
    main_toolbar_->addAction(action);
  };
  const auto add_legacy = [&](QAction* action, int index) {
    toolbar_icons_.emplace_back(action, index);
    main_toolbar_->addAction(action);
  };
  // Default groups and custom bitmap indices: jwp_stat.cpp:235-325.
  add_standard(new_action, "document-new", tr("New"),
               style()->standardIcon(QStyle::SP_FileIcon));
  add_standard(open_action, "document-open", tr("Open"),
               style()->standardIcon(QStyle::SP_DialogOpenButton));
  add_standard(save_action, "document-save", tr("Save"),
               style()->standardIcon(QStyle::SP_DialogSaveButton));
  main_toolbar_->addSeparator();
  add_standard(print_action_, "document-print", tr("Print"));
  main_toolbar_->addSeparator();
  add_standard(cut_action, "edit-cut", tr("Cut"));
  add_standard(copy_action, "edit-copy", tr("Copy"));
  add_standard(paste_action, "edit-paste", tr("Paste"));
  main_toolbar_->addSeparator();
  add_standard(undo_action_, "edit-undo", tr("Undo"));
  add_standard(redo_action_, "edit-redo", tr("Redo"));
  main_toolbar_->addSeparator();
  add_standard(find_action, "edit-find", tr("Find"));
  add_standard(replace_action, "edit-find-replace", tr("Replace"));
  add_legacy(find_next_action, 21);
  main_toolbar_->addSeparator();
  add_legacy(kana_input_action_, 0);
  add_legacy(ascii_input, 1);
  add_legacy(jascii_input, 2);
  add_legacy(convert_action_, 3);
  main_toolbar_->addSeparator();
  add_legacy(kanji_info_action_, 4);
  add_legacy(jis_table_action_, 13);
  add_legacy(edict_lookup_action_, 19);
  add_legacy(kanji_count_action_, 14);
  main_toolbar_->addSeparator();
  add_legacy(kanji_lookup_action_, 5);
  add_legacy(bushu_lookup_action_, 6);
  add_legacy(stroke_bushu_lookup_action_, 7);
  add_legacy(skip_lookup_action_, 8);
  add_legacy(spahn_lookup_action_, 9);
  add_legacy(four_corner_lookup_action_, 10);
  add_legacy(kanji_reading_lookup_action_, 11);
  add_legacy(index_lookup_action_, 12);
  main_toolbar_->addSeparator();
  add_legacy(page_layout_action_, 18);

  QMenu* view_menu = new QMenu(tr("&View"), this);
  menuBar()->insertMenu(format_menu->menuAction(), view_menu);
  QAction* toolbar_visible = main_toolbar_->toggleViewAction();
  toolbar_visible->setText(tr("&Toolbar"));
  toolbar_visible->setObjectName(QStringLiteral("showToolbarAction"));
  view_menu->addAction(toolbar_visible);
  connect(toolbar_visible, &QAction::triggered, this, [this](bool visible) {
    application_settings_.show_toolbar = visible;
  });
}

QString MainWindow::resource_report() const {
  QStringList lines;
  lines << (wnn_resources_ != nullptr
                ? tr("WNN conversion: loaded (%1 records)")
                      .arg(wnn_resources_->dictionary.records().size())
                : tr("WNN conversion: unavailable (wnn.dix and wnn.dat)"));
  if (wnn_resources_ != nullptr) {
    lines << tr("Conversion preferences: %1")
                 .arg(wnn_resources_->preferences_path)
          << tr("User conversions: %1")
                 .arg(wnn_resources_->user_dictionary_path);
  }
  lines << (kanji_info_database_ != nullptr
                ? tr("Kanji information: loaded (%1 characters)")
                      .arg(kanji_info_database_->count())
                : tr("Kanji information: unavailable (kanjinfo.dat)"))
        << (has_kanji_lookup()
                ? tr("Radical/stroke lookup: loaded")
                : tr("Radical/stroke lookup: unavailable "
                     "(kanjinfo.dat, radical.dat and stroke.dat)"))
        << (radical_sheet_.isNull()
                ? tr("Radical graphics: unavailable (radicals.bmp)")
                : tr("Radical graphics: loaded"));
  if (edict_resources_ == nullptr) {
    lines << tr("Word dictionaries: not configured (dict.cfg)");
  } else {
    lines << tr("Dictionary directory: %1").arg(edict_config_directory_)
          << tr("Word dictionaries: %1 loaded")
                 .arg(edict_resources_->resources.size());
    for (const auto& resource : edict_resources_->resources) {
      lines << tr("  %1: %2 records (%3)")
                    .arg(resource.label)
                    .arg(resource.dictionary.records().size())
                    .arg(resource.source_path);
      const auto& errors = resource.dictionary.record_errors();
      if (!errors.empty()) {
        lines << tr("    %1 invalid records skipped").arg(errors.size());
        for (std::size_t i = 0; i < std::min<std::size_t>(errors.size(), 5); ++i) {
          lines << QStringLiteral("    ") + QString::fromStdString(errors[i]);
        }
      }
    }
    for (const auto& failure : edict_resources_->failures) {
      lines << tr("  Unavailable: %1: %2")
                   .arg(failure.source_path, failure.message);
    }
    if (edict_resources_->truncated) {
      lines << tr("Dictionary resource limits were reached.");
    }
  }
  lines << (recent_files_path_.isEmpty() ? tr("Recent files: memory only")
                                        : tr("Recent files: %1").arg(recent_files_path_));
  if (!recent_file_warning_.isEmpty()) lines << recent_file_warning_;
  lines << (application_settings_path_.isEmpty() ? tr("Settings: memory only")
      : tr("Settings: %1").arg(application_settings_path_));
  if (!application_settings_warning_.isEmpty()) lines << application_settings_warning_;
  if (!project_path_.isEmpty()) lines << tr("Project: %1").arg(project_path_);
  if (!project_warning_.isEmpty()) lines << project_warning_;
  lines << application_font_warnings_;
  if (!application_settings_.unapplied.isEmpty())
    lines << tr("Retained settings not applied by the native interface: %1")
                 .arg(application_settings_.unapplied.join(QStringLiteral(", ")));
  lines << QString()
        << tr("Use --config-dir for settings and dictionaries, and "
              "--user-data-dir for conversion learning. WNN files are found "
              "in the config directory unless --wnn-data-dir overrides it.")
        << tr("Qt platform/style: %1 / %2")
               .arg(QGuiApplication::platformName(), style()->objectName())
        << tr("Menu colors (background/text/button text): %1 / %2 / %3")
               .arg(menuBar()->palette().color(QPalette::Window).name(),
                    menuBar()->palette().color(QPalette::WindowText).name(),
                    menuBar()->palette().color(QPalette::ButtonText).name());
  return lines.join(QLatin1Char('\n'));
}

void MainWindow::update_resource_status() {
  const bool dictionaries_loaded =
      edict_resources_ != nullptr && !edict_resources_->truncated &&
      std::any_of(edict_resources_->resources.begin(),
                  edict_resources_->resources.end(), [](const auto& resource) {
                    return resource.entry.special !=
                           core::EdictRegistrySpecial::kUser;
                  }) &&
      std::none_of(edict_resources_->failures.begin(),
                   edict_resources_->failures.end(), [this](const auto& failure) {
                     return !failure.quiet ||
                            edict_resources_->registry.entries
                                    [failure.registry_index].special !=
                                core::EdictRegistrySpecial::kUser;
                   });
  const bool record_warnings =
      edict_resources_ != nullptr &&
      std::any_of(edict_resources_->resources.begin(),
                  edict_resources_->resources.end(), [](const auto& resource) {
                    return !resource.dictionary.record_errors().empty();
                  });
  resource_status_button_->setText(
      wnn_resources_ != nullptr && has_kanji_lookup() && dictionaries_loaded
          ? (record_warnings || !recent_file_warning_.isEmpty() ||
             !application_settings_warning_.isEmpty() || !application_font_warnings_.isEmpty() ||
              !application_settings_.unapplied.isEmpty() || !project_warning_.isEmpty()
                 ? tr("Resources: warnings") : tr("Resources: loaded"))
          : tr("Resources: incomplete"));
  resource_status_button_->setToolTip(
      !project_warning_.isEmpty() ? project_warning_ :
      !application_settings_warning_.isEmpty() ? application_settings_warning_ :
      !recent_file_warning_.isEmpty() ? recent_file_warning_ :
      !application_font_warnings_.isEmpty() ? application_font_warnings_.join(QLatin1Char('\n')) :
      !application_settings_.unapplied.isEmpty() ? tr("Some imported settings are retained but not yet applied") :
      tr("Inspect dictionary and lookup data"));
}

void MainWindow::undo_document() {
  finish_kana_input();
  if (!document_->jwp_document_.has_value()) {
    document_->editor_->undo();
    return;
  }
  try {
    core::JwpDocumentModel model = *document_->jwp_document_;
    core::JwpDocumentHistory history = document_->jwp_history_;
    core::JwpPosition caret = document_->jwp_caret_.value_or(core::JwpPosition{});
    if (!history.undo(model, caret)) {
      return;
    }
    document_->jwp_document_ = std::move(model);
    document_->jwp_history_ = std::move(history);
    restore_jwp_history_state(caret);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not undo: %1").arg(QString::fromUtf8(error.what())), 5000);
  }
}

void MainWindow::redo_document() {
  finish_kana_input();
  if (!document_->jwp_document_.has_value()) {
    document_->editor_->redo();
    return;
  }
  try {
    core::JwpDocumentModel model = *document_->jwp_document_;
    core::JwpDocumentHistory history = document_->jwp_history_;
    core::JwpPosition caret = document_->jwp_caret_.value_or(core::JwpPosition{});
    if (!history.redo(model, caret)) {
      return;
    }
    document_->jwp_document_ = std::move(model);
    document_->jwp_history_ = std::move(history);
    restore_jwp_history_state(caret);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not redo: %1").arg(QString::fromUtf8(error.what())), 5000);
  }
}

void MainWindow::restore_jwp_history_state(core::JwpPosition caret) {
  std::u32string text =
      core::decode_jwp_plain_text(*document_->jwp_document_, document_->jwp_code_page_);
  const QString qt_text = to_qstring(text);
  const std::size_t offset =
      core::jwp_plain_text_offset(*document_->jwp_document_, caret);
  const int qt_offset = utf16_offset_for_utf32(text, offset);

  document_->updating_editor_ = true;
  document_->editor_->setPlainText(qt_text);
  apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
  QTextCursor cursor = document_->editor_->textCursor();
  cursor.setPosition(qt_offset);
  document_->editor_->setTextCursor(cursor);
  document_->updating_editor_ = false;

  document_->rendered_jwp_text_ = std::move(text);
  document_->jwp_caret_ = caret;
  document_->expected_jwp_caret_.reset();
  const bool modified = !document_->saved_jwp_document_.has_value() ||
                        document_->jwp_document_->document() != *document_->saved_jwp_document_;
  document_->editor_->document()->setModified(modified);
  update_undo_actions();
  update_title();
}

void MainWindow::apply_jwp_presentation(const core::JwpDocument& document,
                                        core::LegacyCodePage code_page) {
  document_->editor_->apply_jwp_layout(document);
  document_->editor_->apply_kanji_colors(document, kanji_color_list_,
                              kanji_color_policy_, code_page);
}

void MainWindow::clear_jwp_presentation() {
  document_->editor_->clear_jwp_layout();
  document_->editor_->clear_kanji_colors();
}

void MainWindow::update_undo_actions() {
  if (undo_action_ == nullptr || redo_action_ == nullptr) {
    return;
  }
  if (conversion_active()) {
    undo_action_->setEnabled(false);
    redo_action_->setEnabled(false);
    return;
  }
  const bool jwp = document_->jwp_document_.has_value();
  undo_action_->setEnabled(jwp ? document_->jwp_history_.can_undo()
                               : document_->qt_undo_available_);
  redo_action_->setEnabled(jwp ? document_->jwp_history_.can_redo()
                               : document_->qt_redo_available_);
}

void MainWindow::update_conversion_actions() {
  if (convert_action_ == nullptr) {
    return;
  }
  const bool active = conversion_active();
  bool can_convert = false;
  if (!active && document_->jwp_document_.has_value() && wnn_resources_ != nullptr) {
    const QTextCursor cursor = document_->editor_->textCursor();
    if (cursor.hasSelection()) {
      try {
        const QString text = document_plain_text(*document_->editor_->document());
        const core::JwpPosition begin = core::jwp_plain_text_position(
            *document_->jwp_document_,
            utf32_offset_for_utf16(text, cursor.selectionStart()));
        const core::JwpPosition end = core::jwp_plain_text_position(
            *document_->jwp_document_,
            utf32_offset_for_utf16(text, cursor.selectionEnd()));
        can_convert = begin.paragraph == end.paragraph && begin != end;
      } catch (const std::exception&) {
      }
    } else {
      can_convert = document_->automatic_conversion_range_.has_value() ||
                    document_->kana_input_.pending_ambiguous();
    }
  }
  convert_action_->setEnabled(active || can_convert);
  conversion_candidates_->setVisible(active && application_settings_.show_kanji_bar);
  if (!active && conversion_candidates_->count() != 0) {
    const QSignalBlocker blocker(conversion_candidates_);
    conversion_candidates_->clear();
  }
  previous_candidate_action_->setEnabled(active);
  next_candidate_action_->setEnabled(active);
  accept_candidate_action_->setEnabled(active);
  print_action_->setEnabled(!active);
  printer_setup_action_->setEnabled(!active);
  user_dictionary_action_->setEnabled(!active && wnn_resources_ != nullptr);
  const bool has_paragraphs = document_->jwp_document_.has_value() &&
                              document_->jwp_document_->paragraph_count() != 0;
  format_file_action_->setEnabled(!active && has_paragraphs);
  format_paragraph_action_->setEnabled(!active && has_paragraphs);
  page_layout_action_->setEnabled(!active && document_->jwp_document_.has_value());
  insert_page_break_action_->setEnabled(!active && document_->jwp_document_.has_value());
  update_kanji_color_actions();
  update_edict_actions();
  update_kanji_info_action();
  update_jis_table_action();
  update_kanji_count_action();
  update_kanji_code_lookup_actions();
  update_kanji_reading_lookup_action();
  update_kanji_lookup_action();
  update_kana_input_state();
}

void MainWindow::update_edict_actions() {
  if (edict_lookup_action_ == nullptr) {
    return;
  }
  bool available =
      edict_resources_ != nullptr && !edict_resources_->resources.empty();
  if (!available && edict_resources_ != nullptr) {
    for (const core::EdictRegistryEntry& entry :
         edict_resources_->registry.entries) {
      if (entry.searched &&
          entry.special != core::EdictRegistrySpecial::kUser) {
        available = true;
        break;
      }
    }
  }
  edict_lookup_action_->setEnabled(available);
  if (edict_results_action_ != nullptr) {
    edict_results_action_->setEnabled(edict_results_window_ != nullptr);
  }
  if (edict_user_dictionary_action_ != nullptr) {
    edict_user_dictionary_action_->setEnabled(
        edict_user_resources_ != nullptr && !conversion_active());
  }
}

void MainWindow::update_kanji_info_action() {
  if (kanji_info_action_ != nullptr) {
    kanji_info_action_->setEnabled(character_target(*document_->editor_).has_value());
  }
}

void MainWindow::update_jis_table_action() {
  if (jis_table_action_ != nullptr) {
    jis_table_action_->setEnabled(!conversion_active());
  }
}

void MainWindow::update_kanji_count_action() {
  if (kanji_count_action_ != nullptr) {
    kanji_count_action_->setEnabled(!conversion_active());
  }
}

void MainWindow::update_kanji_code_lookup_actions() {
  const bool enabled = kanji_info_database_ != nullptr &&
                       !conversion_active();
  if (skip_lookup_action_ != nullptr) skip_lookup_action_->setEnabled(enabled);
  if (four_corner_lookup_action_ != nullptr)
    four_corner_lookup_action_->setEnabled(enabled);
  if (bushu_lookup_action_ != nullptr)
    bushu_lookup_action_->setEnabled(enabled);
  if (stroke_bushu_lookup_action_ != nullptr)
    stroke_bushu_lookup_action_->setEnabled(enabled);
  if (spahn_lookup_action_ != nullptr)
    spahn_lookup_action_->setEnabled(enabled);
  if (index_lookup_action_ != nullptr)
    index_lookup_action_->setEnabled(enabled);
}

void MainWindow::update_kanji_reading_lookup_action() {
  if (kanji_reading_lookup_action_ != nullptr) {
    kanji_reading_lookup_action_->setEnabled(
        kanji_info_database_ != nullptr && !conversion_active());
  }
}

void MainWindow::update_kanji_lookup_action() {
  if (kanji_lookup_action_ != nullptr) {
    kanji_lookup_action_->setEnabled(has_kanji_lookup() &&
                                     !conversion_active());
  }
}

void MainWindow::update_kanji_color_actions() {
  if (kanji_color_options_action_ == nullptr) {
    return;
  }
  const bool active = conversion_active();
  const bool configured = !kanji_color_list_path_.isEmpty();
  const bool jwp = document_->jwp_document_.has_value();
  kanji_color_options_action_->setEnabled(
      !kanji_color_settings_path_.isEmpty());
  make_kanji_color_list_action_->setEnabled(configured && jwp && !active);
  append_kanji_color_list_action_->setEnabled(configured && jwp && !active);
  edit_kanji_color_list_action_->setEnabled(configured && !active);
  view_kanji_color_list_action_->setEnabled(configured && !active &&
                                             !kanji_color_list_.empty());
  clear_kanji_color_list_action_->setEnabled(configured && !active &&
                                              !kanji_color_list_.empty());
}

bool MainWindow::kana_input_enabled() const noexcept {
  return document_->jwp_document_.has_value() && document_->input_mode_ == InputMode::kKanji;
}

void MainWindow::set_input_mode(InputMode mode) {
  if (!document_->jwp_document_.has_value()) {
    return;
  }
  if (document_->input_mode_ != mode) {
    if (conversion_active() && !accept_conversion()) {
      update_kana_input_state();
      return;
    }
    finish_kana_input();
    if (conversion_active() && !accept_conversion()) {
      update_kana_input_state();
      return;
    }
    reset_kana_input(false);
    document_->input_mode_ = mode;
    document_->jwp_history_.break_coalescing();
  }
  update_kana_input_state();
}

void MainWindow::update_kana_input_state() {
  if (kana_input_action_ == nullptr || input_mode_button_ == nullptr) {
    return;
  }
  const bool enabled = document_->jwp_document_.has_value();
  japanese_editing_action_->setChecked(enabled);
  japanese_editing_action_->setEnabled(!conversion_active() && !document_->kana_input_.pending());
  input_mode_actions_->setEnabled(enabled);
  toggle_input_mode_action_->setEnabled(enabled);
  input_mode_button_->setEnabled(enabled);
  for (QAction* action : input_mode_actions_->actions()) {
    action->setChecked(enabled &&
                       action->data().toInt() == static_cast<int>(document_->input_mode_));
  }
  if (!enabled) {
    input_mode_button_->setText(tr("Unicode Text"));
    return;
  }
  switch (document_->input_mode_) {
    case InputMode::kKanji: input_mode_button_->setText(tr("Kanji")); break;
    case InputMode::kAscii: input_mode_button_->setText(tr("ASCII")); break;
    case InputMode::kJascii: input_mode_button_->setText(tr("JASCII")); break;
  }
}

void MainWindow::apply_kana_input_events(
    const std::vector<core::KanaInputEvent>& events) {
  if (events.empty() || !document_->jwp_document_.has_value() || document_->editor_->isReadOnly()) {
    return;
  }

  const QScopedValueRollback<bool> applying(document_->applying_kana_input_, true);
  const bool replace_selection = document_->editor_->textCursor().hasSelection();
  bool force_after_events = false;
  for (std::size_t index = 0; index < events.size(); ++index) {
    core::KanaInputEvent event = events[index];
    // Compound kana is emitted one character at a time. Keep one syllable's
    // replacement in one undo step without merging conversion-start boundaries.
    while (index + 1 < events.size() &&
           ((event.kind != core::KanaInputKind::kText &&
             events[index + 1].kind == core::KanaInputKind::kKanaContinue) ||
            (event.kind == core::KanaInputKind::kText &&
             events[index + 1].kind == core::KanaInputKind::kText &&
             !document_->automatic_conversion_range_))) {
      const auto& following = events[++index].text;
      event.text.insert(event.text.end(), following.begin(), following.end());
    }
    if (event.text.empty()) {
      continue;
    }

    if (event.kind == core::KanaInputKind::kKanaStart &&
        document_->automatic_conversion_range_.has_value()) {
      if (attempt_automatic_conversion(true)) {
        accept_conversion();
      }
      clear_automatic_conversion_range();
    }

    QTextCursor cursor = document_->editor_->textCursor();
    const QString before = document_plain_text(*document_->editor_->document());
    core::JwpPosition insertion_begin = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(before, cursor.selectionStart()));
    bool extends_automatic =
        document_->automatic_conversion_range_.has_value() && !cursor.hasSelection() &&
        insertion_begin == document_->automatic_conversion_range_->end;
    if (document_->automatic_conversion_range_.has_value() && !extends_automatic) {
      if (attempt_automatic_conversion(true)) {
        accept_conversion();
      }
      clear_automatic_conversion_range();
      cursor = document_->editor_->textCursor();
      const QString current = document_plain_text(*document_->editor_->document());
      insertion_begin = core::jwp_plain_text_position(
          *document_->jwp_document_,
          utf32_offset_for_utf16(current, cursor.selectionStart()));
      extends_automatic = false;
    }

    document_->editor_->insert_composed_text(
        core::decode_jwp_text(event.text, document_->jwp_code_page_), !replace_selection);
    if (!document_->jwp_caret_.has_value() ||
        document_->jwp_caret_->paragraph != insertion_begin.paragraph ||
        document_->jwp_caret_->offset < insertion_begin.offset) {
      throw core::JwpConversionError(
          "kana input did not produce a valid document range");
    }

    if (event.kind == core::KanaInputKind::kKanaStart) {
      document_->automatic_conversion_range_ =
          core::JwpRange{insertion_begin, *document_->jwp_caret_};
      force_after_events = false;
    } else if (event.kind == core::KanaInputKind::kKanaContinue &&
               extends_automatic) {
      document_->automatic_conversion_range_->end = *document_->jwp_caret_;
    } else if (event.kind == core::KanaInputKind::kText &&
               extends_automatic) {
      force_after_events = true;
    }
  }

  if (document_->automatic_conversion_range_.has_value()) {
    attempt_automatic_conversion(force_after_events);
  }
}

void MainWindow::finish_kana_input() {
  if (document_->kana_input_.pending()) {
    try {
      apply_kana_input_events(document_->kana_input_.flush());
    } catch (const core::KanaInputError&) {
      document_->kana_input_.discard();
    }
  }
  if (document_->automatic_conversion_range_.has_value()) {
    attempt_automatic_conversion(true);
  }
  if (conversion_active()) {
    accept_conversion();
  }
}

void MainWindow::reset_kana_input(bool disable_mode) {
  document_->kana_input_.discard();
  clear_automatic_conversion_range();
  if (disable_mode) {
    document_->input_mode_ = InputMode::kAscii;
  }
  update_kana_input_state();
}

bool MainWindow::attempt_automatic_conversion(bool force) {
  if (!document_->automatic_conversion_range_.has_value()) {
    return false;
  }
  if (!document_->jwp_document_.has_value() || wnn_resources_ == nullptr) {
    clear_automatic_conversion_range();
    return false;
  }

  try {
    const core::JwpRange pending_range = *document_->automatic_conversion_range_;
    if (pending_range.begin.paragraph != pending_range.end.paragraph ||
        pending_range.begin.paragraph >=
            document_->jwp_document_->document().paragraphs.size()) {
      throw core::JwpConversionError(
          "automatic conversion range is invalid");
    }
    const core::JwpText& paragraph =
        document_->jwp_document_->document()
            .paragraphs[pending_range.begin.paragraph]
            .text;
    if (pending_range.begin.offset > pending_range.end.offset ||
        pending_range.end.offset > paragraph.size()) {
      throw core::JwpConversionError(
          "automatic conversion range is out of bounds");
    }
    const core::JwpText input(
        paragraph.begin() + pending_range.begin.offset,
        paragraph.begin() + pending_range.end.offset);
    core::WnnAutomaticPreparation automatic =
        wnn_resources_->session.prepare_automatic(input);
    if (automatic.wait_for_more && !force) {
      show_automatic_conversion_range();
      statusBar()->showMessage(tr("Waiting for more kana"));
      return false;
    }

    std::size_t matched_length = automatic.matched_length;
    std::optional<core::WnnPreparedConversion> prepared;
    if (automatic.conversion.has_value()) {
      prepared.emplace(std::move(*automatic.conversion));
    } else if (automatic.wait_for_more && force) {
      const std::size_t maximum =
          std::min(input.size(), core::kWnnMaximumKeySize);
      for (std::size_t length = maximum; length > 0; --length) {
        core::JwpText prefix(input.begin(), input.begin() + length);
        prepared = wnn_resources_->session.prepare(prefix);
        if (prepared.has_value()) {
          matched_length = length;
          break;
        }
      }
    }
    if (!prepared.has_value()) {
      clear_automatic_conversion_range();
      return false;
    }
    if (matched_length == 0 ||
        matched_length > pending_range.end.offset - pending_range.begin.offset) {
      throw core::JwpConversionError(
          "automatic conversion produced an invalid prefix length");
    }

    const core::JwpRange conversion_range{
        pending_range.begin,
        {pending_range.begin.paragraph,
         pending_range.begin.offset + matched_length}};
    core::JwpPosition caret = document_->jwp_caret_.value_or(pending_range.end);
    if (caret.paragraph != conversion_range.begin.paragraph ||
        caret.offset < conversion_range.end.offset) {
      caret = conversion_range.end;
    }

    auto transaction = std::make_unique<core::JwpConversionTransaction>(
        *document_->jwp_document_, document_->jwp_history_, wnn_resources_->session);
    document_->conversion_preferences_before_ = wnn_resources_->preferences;
    transaction->begin_prepared(conversion_range, caret,
                                std::move(*prepared));
    clear_automatic_conversion_range();
    document_->jwp_conversion_ = std::move(transaction);
    document_->editor_->setReadOnly(true);
    restore_jwp_conversion_state();
    return true;
  } catch (const std::exception& error) {
    if (conversion_active()) {
      rollback_conversion_noexcept();
    } else {
      document_->conversion_preferences_before_.reset();
    }
    clear_automatic_conversion_range();
    statusBar()->showMessage(
        tr("Could not convert kana: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

void MainWindow::clear_automatic_conversion_range() {
  document_->automatic_conversion_range_.reset();
  if (!conversion_active()) {
    document_->editor_->set_transient_extra_selections({});
  }
  update_conversion_actions();
}

void MainWindow::show_automatic_conversion_range() {
  if (!document_->automatic_conversion_range_.has_value() ||
      !document_->jwp_document_.has_value()) {
    document_->editor_->set_transient_extra_selections({});
    return;
  }
  const std::u32string text =
      core::decode_jwp_plain_text(*document_->jwp_document_, document_->jwp_code_page_);
  const core::JwpRange range = *document_->automatic_conversion_range_;
  QTextCursor cursor(document_->editor_->document());
  cursor.setPosition(utf16_offset_for_utf32(
      text, core::jwp_plain_text_offset(*document_->jwp_document_, range.begin)));
  cursor.setPosition(utf16_offset_for_utf32(
                         text, core::jwp_plain_text_offset(*document_->jwp_document_,
                                                          range.end)),
                     QTextCursor::KeepAnchor);
  QTextEdit::ExtraSelection selection;
  selection.cursor = cursor;
  QColor highlight = document_->editor_->palette().color(QPalette::Highlight);
  highlight.setAlpha(80);
  selection.format.setBackground(highlight);
  document_->editor_->set_transient_extra_selections({selection});
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == document_->editor_ || watched == document_->editor_->viewport()) {
    if (event->type() == QEvent::ContextMenu) {
      show_character_context_menu(*document_->editor_, *static_cast<QContextMenuEvent*>(event),
          [this](CharacterTarget target) { show_kanji_info_dialog(target); });
      return true;
    }
    if (event->type() == QEvent::MouseButtonPress &&
        static_cast<QMouseEvent*>(event)->button() == Qt::RightButton)
      return true;
  }
  if (watched != document_->editor_ || !document_->jwp_document_.has_value() ||
      conversion_active() || document_->editor_->isReadOnly()) {
    return QMainWindow::eventFilter(watched, event);
  }
  if (document_->input_mode_ == InputMode::kJascii && event->type() == QEvent::KeyPress) {
    const auto* key = static_cast<QKeyEvent*>(event);
    const QString text = key->text();
    if (!(key->modifiers() &
          (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) &&
        text.size() == 1 && text.front().unicode() >= 0x20 &&
        text.front().unicode() <= 0xff && text.front().unicode() != 0x7f) {
      const auto code = core::ascii_to_jascii(
          static_cast<char>(text.front().unicode()), true);
      if (code.has_value()) {
        document_->editor_->insert_composed_text(
            core::decode_jwp_text({*code}, document_->jwp_code_page_));
      }
      return true;
    }
  }
  if (!kana_input_enabled()) {
    return QMainWindow::eventFilter(watched, event);
  }
  if (event->type() == QEvent::MouseButtonPress) {
    finish_kana_input();
    return QMainWindow::eventFilter(watched, event);
  }
  if (event->type() != QEvent::KeyPress) {
    return QMainWindow::eventFilter(watched, event);
  }

  auto* key_event = static_cast<QKeyEvent*>(event);
  if (document_->kana_input_.pending() &&
      (key_event->key() == Qt::Key_Backspace ||
       key_event->key() == Qt::Key_Delete ||
       key_event->key() == Qt::Key_Escape)) {
    document_->kana_input_.discard();
    statusBar()->showMessage(tr("Discarded pending kana input"), 1500);
    update_conversion_actions();
    return true;
  }

  const Qt::KeyboardModifiers command_modifiers =
      key_event->modifiers() &
      (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
  const QString text = key_event->text();
  if (command_modifiers == Qt::NoModifier && text.size() == 1) {
    const ushort value = text.at(0).unicode();
    if (value >= 0x20U && value <= 0x7eU) {
      try {
        apply_kana_input_events(
            document_->kana_input_.push_ascii(static_cast<char>(value)));
      } catch (const std::exception& error) {
        document_->kana_input_.discard();
        statusBar()->showMessage(
            tr("Could not compose kana: %1")
                .arg(QString::fromUtf8(error.what())),
            5000);
      }
      update_conversion_actions();
      return true;
    }
  }

  if (document_->kana_input_.pending() && key_event->key() != Qt::Key_Shift &&
      key_event->key() != Qt::Key_Control && key_event->key() != Qt::Key_Alt &&
      key_event->key() != Qt::Key_Meta &&
      key_event->key() != Qt::Key_CapsLock) {
    finish_kana_input();
  } else if (document_->automatic_conversion_range_.has_value() &&
             key_event->key() != Qt::Key_Shift &&
             key_event->key() != Qt::Key_Control &&
             key_event->key() != Qt::Key_Alt &&
             key_event->key() != Qt::Key_Meta &&
             key_event->key() != Qt::Key_CapsLock) {
    finish_kana_input();
  }
  return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::load_wnn_resources(const QString& index_path,
                                    const QString& data_path,
                                    const QString& preferences_path,
                                    OpenMode mode) {
  return load_wnn_resources(index_path, data_path, preferences_path, {}, mode);
}

bool MainWindow::load_wnn_resources(const QString& index_path,
                                    const QString& data_path,
                                    const QString& preferences_path,
                                    const QString& user_dictionary_path,
                                    OpenMode mode) {
  if (conversion_active()) {
    if (mode == OpenMode::kInteractive) {
      statusBar()->showMessage(
          tr("Accept the current conversion before changing dictionaries"),
          5000);
    }
    return false;
  }
  finish_kana_input();
  try {
    core::WnnDictionary dictionary = core::WnnDictionary::parse(
        read_file_bytes(index_path), read_file_bytes(data_path));
    std::optional<core::WnnPreferences> loaded_preferences =
        read_wnn_preferences_file(preferences_path);
    core::WnnPreferences preferences =
        loaded_preferences ? std::move(*loaded_preferences)
                            : core::WnnPreferences{};
    std::optional<core::WnnUserDictionary> loaded_user_dictionary;
    if (!user_dictionary_path.isEmpty()) {
      loaded_user_dictionary =
          read_wnn_user_dictionary_file(user_dictionary_path);
    }
    core::WnnUserDictionary user_dictionary =
        loaded_user_dictionary ? std::move(*loaded_user_dictionary)
                               : core::WnnUserDictionary{};

    auto resources = std::make_unique<WnnResources>(
        std::move(dictionary), std::move(preferences),
        std::move(user_dictionary), preferences_path, user_dictionary_path);
    delete wnn_user_dictionary_dialog_;
    wnn_user_dictionary_dialog_ = nullptr;
    wnn_resources_ = std::move(resources);
    update_resource_status();
    update_conversion_actions();
    statusBar()->showMessage(tr("Loaded WNN conversion dictionaries"), 3000);
    return true;
  } catch (const std::exception& error) {
    update_conversion_actions();
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not load WNN dictionaries"), error);
    }
    return false;
  }
}

const core::WnnUserDictionary* MainWindow::wnn_user_dictionary()
    const noexcept {
  return wnn_resources_ == nullptr ? nullptr
                                   : &wnn_resources_->user_dictionary;
}

bool MainWindow::set_wnn_user_dictionary(
    core::WnnUserDictionary user_dictionary, OpenMode mode) {
  if (conversion_active()) {
    if (mode == OpenMode::kInteractive) {
      statusBar()->showMessage(
          tr("Accept the current conversion before changing user entries"),
          5000);
    }
    return false;
  }
  finish_kana_input();
  if (conversion_active()) {
    return false;
  }
  try {
    if (wnn_resources_ == nullptr ||
        wnn_resources_->user_dictionary_path.isEmpty()) {
      throw std::runtime_error("WNN user dictionary path is not configured");
    }

    auto candidate = std::make_unique<WnnResources>(
        wnn_resources_->dictionary, wnn_resources_->preferences,
        std::move(user_dictionary), wnn_resources_->preferences_path,
        wnn_resources_->user_dictionary_path);
    write_wnn_user_dictionary_file(candidate->user_dictionary_path,
                                   candidate->user_dictionary);
    wnn_resources_ = std::move(candidate);
    clear_automatic_conversion_range();
    update_conversion_actions();
    statusBar()->showMessage(tr("Updated user conversion dictionary"), 3000);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not update user conversion dictionary"), error);
    }
    return false;
  }
}

void MainWindow::show_wnn_user_dictionary_dialog() {
  if (conversion_active() || wnn_resources_ == nullptr) {
    statusBar()->showMessage(tr("WNN user conversions are not available"),
                             3000);
    return;
  }
  finish_kana_input();
  if (conversion_active() || wnn_resources_ == nullptr) {
    return;
  }
  if (wnn_user_dictionary_dialog_ != nullptr) {
    wnn_user_dictionary_dialog_->show();
    wnn_user_dictionary_dialog_->raise();
    wnn_user_dictionary_dialog_->activateWindow();
    return;
  }

  auto* dialog = new WnnUserDictionaryDialog(
      wnn_resources_->user_dictionary,
      [this](core::WnnUserDictionary dictionary) {
        return set_wnn_user_dictionary(std::move(dictionary));
      },
      [this](const core::WnnUserEntry& entry) {
        if (!insert_wnn_user_entry(entry)) {
          throw std::runtime_error(
              "Could not insert the user conversion into the document");
        }
      },
      this);
  dialog->setObjectName(QStringLiteral("userDictionaryDialog"));
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { wnn_user_dictionary_dialog_ = nullptr; });
  wnn_user_dictionary_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_edict_user_dictionary_dialog() {
  if (conversion_active() || edict_user_resources_ == nullptr) {
    statusBar()->showMessage(tr("EDICT user dictionary is not available"),
                             3000);
    return;
  }
  finish_kana_input();
  if (conversion_active() || edict_user_resources_ == nullptr) {
    return;
  }
  if (edict_user_dictionary_dialog_ != nullptr) {
    edict_user_dictionary_dialog_->show();
    edict_user_dictionary_dialog_->raise();
    edict_user_dictionary_dialog_->activateWindow();
    return;
  }

  auto* dialog = new EdictUserDictionaryDialog(
      edict_user_resources_->dictionary, edict_user_resources_->code_page,
      [this](core::EdictUserDictionary dictionary) {
        return set_edict_user_dictionary(std::move(dictionary));
      },
      [this](const core::EdictUserEntry& entry) {
        if (!insert_edict_user_entry(entry)) {
          throw std::runtime_error(
              "Could not insert the user dictionary entry into the document");
        }
      },
      this);
  dialog->setObjectName(QStringLiteral("edictUserDictionaryDialog"));
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { edict_user_dictionary_dialog_ = nullptr; });
  edict_user_dictionary_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_kanji_info_dialog(std::optional<CharacterTarget> target,
                                       std::optional<core::JisCode> code) {
  if (!target && !code) target = character_target(*document_->editor_);
  if (!target && !code) {
    statusBar()->showMessage(tr("No character is available here"), 3000);
    return;
  }
  if (!code && target && document_->jwp_document_) {
    const QString current = document_plain_text(*document_->editor_->document());
    const int width = target->character > 0xffff ? 2 : 1;
    if (target->position >= 0 &&
        static_cast<qsizetype>(target->position) + width <= current.size() &&
        current.mid(target->position, width) ==
            to_qstring(std::u32string{target->character})) {
      const auto position = core::jwp_plain_text_position(*document_->jwp_document_,
          utf32_offset_for_utf16(current, target->position));
      const auto& paragraph = document_->jwp_document_->document().paragraphs[position.paragraph];
      if (position.offset < paragraph.text.size()) code = paragraph.text[position.offset];
    }
  }
  const auto previous = findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog"),
                                               Qt::FindDirectChildrenOnly);
  auto* dialog = new KanjiInfoDialog(kanji_info_database_.get(),
      [this](char32_t character) { show_kanji_info_dialog(CharacterTarget{character, -1}); },
      this, [this](std::u32string text) { return insert_edict_text(std::move(text)); });
  dialog->set_options(application_settings_.kanji_info);
  if (!(code ? dialog->set_code(*code, document_->jwp_code_page_)
             : dialog->set_character(target->character, document_->jwp_code_page_))) {
    delete dialog;
    statusBar()->showMessage(tr("This character cannot be displayed"), 3000);
    return;
  }
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  if (!previous.empty()) {
    const QRect available = previous.back()->screen()->availableGeometry();
    QPoint next = previous.back()->pos() + QPoint(24, 24);
    if (next.x() + dialog->frameGeometry().width() > available.right() + 1)
      next.setX(available.left());
    if (next.y() + dialog->frameGeometry().height() > available.bottom() + 1)
      next.setY(available.top());
    next.setX(std::max(next.x(), available.left()));
    next.setY(std::max(next.y(), available.top()));
    dialog->move(next);
  }
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::show_kanji_info_code(core::JisCode code) {
  show_kanji_info_dialog({}, code);
}

void MainWindow::show_jis_table_dialog() {
  if (conversion_active()) {
    statusBar()->showMessage(tr("JIS table is not available"), 3000);
    return;
  }
  const std::optional<core::JisCode> seed = jwp_character_target();
  if (jis_table_dialog_ != nullptr) {
    if (seed.has_value()) (void)jis_table_dialog_->set_jis(*seed);
    jis_table_dialog_->show();
    jis_table_dialog_->raise();
    jis_table_dialog_->activateWindow();
    return;
  }
  auto* dialog = new JisTableDialog(
      [this](const core::JisTableEntry& entry) {
        if (!insert_edict_text(core::decode_jwp_text({entry.jis}))) {
          throw std::runtime_error(
              "Could not insert the JIS table character into the document");
        }
      },
      [this](core::JisCode code) { show_kanji_info_code(code); }, this);
  if (seed.has_value()) (void)dialog->set_jis(*seed);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { jis_table_dialog_ = nullptr; });
  jis_table_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_kanji_count_dialog() {
  if (document_->updating_editor_ || document_->applying_kana_input_ ||
      conversion_active()) {
    statusBar()->showMessage(tr("Count Kanji is not available"), 3000);
    return;
  }
  try {
    if (kanji_count_dialog_ == nullptr) {
      const core::JwpDocument empty;
      auto* dialog = new KanjiCountDialog(
          {&empty}, kanji_color_list_, kanji_info_database_.get(),
          [this](std::u32string text) { insert_edict_text(std::move(text)); },
          [this](core::JisCode code) { show_kanji_info_code(code); }, this);
      dialog->set_document_provider([this] {
        if (conversion_active() || !finish_document_input())
          throw core::KanjiCountError("Count Kanji is not available during input");
        const core::KanjiCountLimits limits;
        if (documents_.size() > limits.documents)
          throw core::KanjiCountError("Kanji count document limit exceeded");
        std::vector<core::JwpDocument> snapshots;
        snapshots.reserve(documents_.size());
        std::size_t remaining = limits.characters;
        const auto current = static_cast<std::size_t>(current_document_index());
        for (std::size_t offset = 0; offset < documents_.size(); ++offset) {
          const auto& state = *documents_[(current + offset) % documents_.size()];
          if (state.jwp_document_) {
            for (const auto& paragraph :
                 state.jwp_document_->document().paragraphs) {
              if (paragraph.text.size() > remaining)
                throw core::KanjiCountError(
                    "Kanji count character limit exceeded");
              remaining -= paragraph.text.size();
            }
            snapshots.push_back(state.jwp_document_->document());
          } else {
            core::JwpDocument snapshot;
            snapshot.paragraphs.emplace_back();
            for (const char32_t value :
                 from_qstring(document_plain_text(*state.editor_->document()))) {
              if (value == U'\n') {
                snapshot.paragraphs.emplace_back();
              } else {
                if (remaining == 0)
                  throw core::KanjiCountError(
                      "Kanji count character limit exceeded");
                --remaining;
                // Count-only geta marks classify unmapped Unicode as "other".
                snapshot.paragraphs.back().text.push_back(
                    value < 0x80
                        ? static_cast<core::JisCode>(value)
                        : core::unicode_to_jis_x0208(value).value_or(0x222e));
              }
            }
            snapshots.push_back(std::move(snapshot));
          }
        }
        return snapshots;
      });
      dialog->setAttribute(Qt::WA_DeleteOnClose);
      connect(dialog, &QObject::destroyed, this,
              [this] { kanji_count_dialog_ = nullptr; });
      kanji_count_dialog_ = dialog;
    }
    kanji_count_dialog_->count();
    kanji_count_dialog_->show();
    kanji_count_dialog_->raise();
    kanji_count_dialog_->activateWindow();
  } catch (const std::exception& error) {
    show_error(tr("Could not open Count Kanji"), error);
  }
}

void MainWindow::show_kanji_code_lookup_dialog(KanjiCodeLookupMode mode) {
  if (kanji_info_database_ == nullptr || conversion_active()) {
    statusBar()->showMessage(tr("Kanji code lookup is not available"), 3000);
    return;
  }
  const auto select_mode = [mode](KanjiCodeLookupDialog& dialog) {
    switch (mode) {
      case KanjiCodeLookupMode::kSkip:
        dialog.select_skip_mode();
        break;
      case KanjiCodeLookupMode::kFourCorner:
        dialog.select_four_corner_mode();
        break;
      case KanjiCodeLookupMode::kBushu:
        dialog.select_bushu_mode();
        break;
      case KanjiCodeLookupMode::kSpahn:
        dialog.select_spahn_mode();
        break;
      case KanjiCodeLookupMode::kStrokeBushu:
        dialog.select_stroke_bushu_mode();
        break;
      case KanjiCodeLookupMode::kIndex:
        dialog.select_index_mode();
        break;
    }
  };
  if (kanji_code_lookup_dialog_ != nullptr) {
    select_mode(*kanji_code_lookup_dialog_);
    kanji_code_lookup_dialog_->show();
    kanji_code_lookup_dialog_->raise();
    kanji_code_lookup_dialog_->activateWindow();
    return;
  }
  auto* dialog = new KanjiCodeLookupDialog(
      *kanji_info_database_,
      [this](const std::vector<core::JisCode>& codes) {
        if (!insert_edict_text(core::decode_jwp_text(codes))) {
          throw std::runtime_error(
              "Could not insert code lookup results into the document");
        }
      },
      [this](core::JisCode code) { show_kanji_info_code(code); }, this,
      radical_sheet_);
  select_mode(*dialog);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { kanji_code_lookup_dialog_ = nullptr; });
  kanji_code_lookup_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_kanji_reading_lookup_dialog() {
  if (kanji_info_database_ == nullptr || conversion_active()) {
    statusBar()->showMessage(tr("Kanji reading lookup is not available"),
                             3000);
    return;
  }
  const std::u32string seed = edict_query_seed();
  if (kanji_reading_lookup_dialog_ != nullptr) {
    if (!seed.empty()) kanji_reading_lookup_dialog_->set_query_text(seed);
    kanji_reading_lookup_dialog_->show();
    kanji_reading_lookup_dialog_->raise();
    kanji_reading_lookup_dialog_->activateWindow();
    return;
  }
  auto* dialog = new KanjiReadingLookupDialog(
      *kanji_info_database_,
      [this](const std::vector<core::JisCode>& codes) {
        if (!insert_edict_text(core::decode_jwp_text(codes))) {
          throw std::runtime_error(
              "Could not insert reading lookup results into the document");
        }
      },
      [this](core::JisCode code) { show_kanji_info_code(code); }, this);
  dialog->set_overwrite_action(overwrite_action_);
  if (!seed.empty()) dialog->set_query_text(seed);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { kanji_reading_lookup_dialog_ = nullptr; });
  kanji_reading_lookup_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_kanji_lookup_dialog() {
  if (!has_kanji_lookup() || conversion_active()) {
    statusBar()->showMessage(tr("Radical lookup is not available"), 3000);
    return;
  }
  if (kanji_lookup_dialog_ != nullptr) {
    kanji_lookup_dialog_->show();
    kanji_lookup_dialog_->raise();
    kanji_lookup_dialog_->activateWindow();
    return;
  }
  auto* dialog = new KanjiLookupDialog(
      *radical_lists_, *stroke_lists_, *kanji_info_database_, radical_sheet_,
      [this](const std::vector<core::JisCode>& codes) {
        if (!insert_edict_text(core::decode_jwp_text(codes))) {
          throw std::runtime_error(
              "Could not insert radical lookup results into the document");
        }
      },
      [this](core::JisCode code) { show_kanji_info_code(code); }, this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { kanji_lookup_dialog_ = nullptr; });
  kanji_lookup_dialog_ = dialog;
  dialog->show();
}

std::optional<core::JisCode> MainWindow::jwp_character_target() const {
  if (!document_->jwp_document_.has_value()) {
    const auto target = character_target(*document_->editor_);
    return target ? core::unicode_to_jis_x0208(target->character) : std::nullopt;
  }
  try {
    const QTextCursor cursor = document_->editor_->textCursor();
    const int qt_offset = cursor.hasSelection() ? cursor.selectionStart()
                                                : cursor.position();
    const core::JwpPosition position = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(document_plain_text(*document_->editor_->document()), qt_offset));
    if (position.paragraph >= document_->jwp_document_->paragraph_count()) {
      return std::nullopt;
    }
    const core::JwpText& text =
        document_->jwp_document_->document().paragraphs[position.paragraph].text;
    std::size_t offset = position.offset;
    if (offset >= text.size()) {
      if (offset == 0) return std::nullopt;
      --offset;
    }
    const core::JisCode code = text[offset];
    return core::describe_jis_character(code).has_value()
               ? std::optional<core::JisCode>(code)
               : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool MainWindow::insert_edict_user_entry(const core::EdictUserEntry& entry) {
  if (edict_user_resources_ == nullptr) {
    return false;
  }
  try {
    return insert_edict_text(core::render_edict_user_entry(entry));
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not insert user dictionary entry: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

bool MainWindow::insert_wnn_user_entry(const core::WnnUserEntry& entry) {
  if (conversion_active() || document_->updating_editor_ ||
      document_->applying_kana_input_ || document_->editor_->isReadOnly()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active()) {
    return false;
  }

  try {
    const core::JwpText inserted = core::render_wnn_user_entry(entry);
    if (!document_->jwp_document_) {
      return insert_edict_text(core::decode_jwp_text(inserted, document_->jwp_code_page_));
    }
    const QString original_text = document_plain_text(*document_->editor_->document());
    const QTextCursor original_cursor = document_->editor_->textCursor();
    const bool original_modified = document_->editor_->document()->isModified();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(original_text, original_cursor.position()));
    const core::JwpPosition selection_begin = core::jwp_plain_text_position(
        *document_->jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionStart()));
    const core::JwpPosition selection_end = core::jwp_plain_text_position(
        *document_->jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionEnd()));

    core::JwpDocumentModel candidate = *document_->jwp_document_;
    core::JwpDocumentHistory history = document_->jwp_history_;
    history.begin(candidate, caret);
    const core::JwpPosition insertion =
        candidate.erase({selection_begin, selection_end});
    candidate.insert(insertion, inserted);
    const core::JwpPosition following{
        insertion.paragraph, insertion.offset + inserted.size()};
    if (!history.commit(candidate, following)) {
      return false;
    }

    std::u32string rendered =
        core::decode_jwp_plain_text(candidate, document_->jwp_code_page_);
    const QString qt_text = to_qstring(rendered);
    const int qt_caret = utf16_offset_for_utf32(
        rendered, core::jwp_plain_text_offset(candidate, following));

    document_->updating_editor_ = true;
    try {
      document_->editor_->setPlainText(qt_text);
      apply_jwp_presentation(candidate.document(), document_->jwp_code_page_);
      QTextCursor cursor(document_->editor_->document());
      cursor.setPosition(qt_caret);
      document_->editor_->setTextCursor(cursor);
    } catch (...) {
      document_->editor_->setPlainText(original_text);
      apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
      document_->editor_->setTextCursor(original_cursor);
      document_->editor_->document()->setModified(original_modified);
      document_->updating_editor_ = false;
      throw;
    }
    document_->updating_editor_ = false;

    document_->jwp_document_ = std::move(candidate);
    document_->jwp_history_ = std::move(history);
    document_->jwp_caret_ = following;
    document_->expected_jwp_caret_.reset();
    document_->rendered_jwp_text_ = std::move(rendered);
    document_->editor_->document()->setModified(
        !document_->saved_jwp_document_.has_value() ||
        document_->jwp_document_->document() != *document_->saved_jwp_document_);
    update_undo_actions();
    update_conversion_actions();
    update_title();
    statusBar()->showMessage(tr("Inserted user conversion"), 2000);
    return true;
  } catch (const std::exception& error) {
    document_->updating_editor_ = false;
    statusBar()->showMessage(
        tr("Could not insert user conversion: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

void MainWindow::show_edict_lookup_dialog() {
  if (edict_resources_ == nullptr) {
    statusBar()->showMessage(tr("Dictionary resources are not available"),
                             3000);
    return;
  }
  finish_kana_input();
  const std::u32string seed = edict_query_seed();
  if (edict_lookup_dialog_ != nullptr) {
    if (!seed.empty()) {
      edict_lookup_dialog_->set_query(seed);
    }
    edict_lookup_dialog_->show();
    edict_lookup_dialog_->raise();
    edict_lookup_dialog_->activateWindow();
    return;
  }

  auto* dialog = new EdictLookupDialog(
      [this](const core::JwpText& query, const EdictLookupOptions& options) {
        if (edict_resources_ == nullptr) {
          throw std::runtime_error("Dictionary resources are not available");
        }
        EdictResourceSearchOptions search;
        search.personal_names = options.personal_names;
        search.place_names = options.place_names;
        search.classical = options.classical;
        EdictResourceSearchReport report = search_edict_resources(
            *edict_resources_, edict_config_directory_, query, search);
        show_edict_results_window(false);
        if (edict_results_window_ != nullptr) {
          edict_results_window_->append_report(report);
        }
        return report;
      },
      [this](const std::u32string& text) { return insert_edict_text(text); },
      this, [this](char32_t character) {
        show_kanji_info_dialog(CharacterTarget{character, -1});
      });
  dialog->set_overwrite_action(overwrite_action_);
  if (!seed.empty()) {
    dialog->set_query(seed);
  }
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { edict_lookup_dialog_ = nullptr; });
  edict_lookup_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_edict_results_window(bool show) {
  if (edict_results_window_ != nullptr) {
    if (show) {
      edict_results_window_->show();
      edict_results_window_->raise();
      edict_results_window_->activateWindow();
    }
    return;
  }

  auto* results = new EdictResultsWindow(this);
  results->set_insert_handler(
      [this](const std::u32string& text) { return insert_edict_text(text); });
  results->setAttribute(Qt::WA_DeleteOnClose);
  connect(results, &QObject::destroyed, this, [this] {
    edict_results_window_ = nullptr;
    if (edict_results_action_ != nullptr) {
      edict_results_action_->setEnabled(false);
    }
  });
  edict_results_window_ = results;
  if (edict_results_action_ != nullptr) {
    edict_results_action_->setEnabled(true);
  }
  if (show) results->show();
}

std::u32string MainWindow::edict_query_seed() const {
  QTextCursor cursor = document_->editor_->textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
    if (!cursor.hasSelection()) {
      const int position = cursor.position();
      const int length = document_->editor_->document()->characterCount() - 1;
      if (position < length) {
        cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
      } else if (position > 0) {
        cursor.setPosition(position - 1);
        cursor.setPosition(position, QTextCursor::KeepAnchor);
      }
    }
  }
  QString selected = cursor.selectedText();
  selected.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
  return from_qstring(selected);
}

bool MainWindow::insert_edict_text(std::u32string_view text) {
  if (text.empty() || conversion_active() || document_->updating_editor_ ||
      document_->applying_kana_input_ || document_->editor_->isReadOnly()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active()) {
    return false;
  }

  try {
    if (!document_->jwp_document_) {
      const QString inserted = to_qstring(text);
      QTextCursor cursor = document_->editor_->textCursor();
      for (const int offset : {cursor.selectionStart(), cursor.selectionEnd()}) {
        if (offset > 0 && document_->editor_->document()->characterAt(offset).isLowSurrogate() &&
            document_->editor_->document()->characterAt(offset - 1).isHighSurrogate())
          throw std::runtime_error("Lookup insertion splits a Unicode surrogate pair");
      }
      const int retained = document_->editor_->document()->characterCount() -
                           (cursor.selectionEnd() - cursor.selectionStart());
      if (inserted.size() > std::numeric_limits<int>::max() - retained)
        throw std::runtime_error("Inserted text exceeds the Qt document limit");
      cursor.beginEditBlock();
      cursor.insertText(inserted);
      cursor.endEditBlock();
      document_->editor_->setTextCursor(cursor);
      document_->editor_->ensureCursorVisible();
      statusBar()->showMessage(tr("Inserted lookup result"), 2000);
      return true;
    }
    const QString original_text = document_plain_text(*document_->editor_->document());
    const QTextCursor original_cursor = document_->editor_->textCursor();
    const bool original_modified = document_->editor_->document()->isModified();
    const std::size_t caret_offset = utf32_offset_for_utf16(
        original_text, original_cursor.position());
    const std::size_t selection_begin = utf32_offset_for_utf16(
        original_text, original_cursor.selectionStart());
    const std::size_t selection_end = utf32_offset_for_utf16(
        original_text, original_cursor.selectionEnd());
    const core::JwpPosition caret =
        core::jwp_plain_text_position(*document_->jwp_document_, caret_offset);

    core::JwpDocumentModel candidate = *document_->jwp_document_;
    core::JwpDocumentHistory history = document_->jwp_history_;
    history.begin(candidate, caret);
    const core::JwpPosition following = core::replace_jwp_plain_text(
        candidate, selection_begin, selection_end - selection_begin, text,
        document_->jwp_code_page_);
    if (!history.commit(candidate, following)) {
      return false;
    }

    std::u32string rendered =
        core::decode_jwp_plain_text(candidate, document_->jwp_code_page_);
    const int qt_caret = utf16_offset_for_utf32(
        rendered, core::jwp_plain_text_offset(candidate, following));
    QScopedValueRollback<bool> update_guard(document_->updating_editor_, true);
    try {
      document_->editor_->setPlainText(to_qstring(rendered));
      apply_jwp_presentation(candidate.document(), document_->jwp_code_page_);
      QTextCursor cursor(document_->editor_->document());
      cursor.setPosition(qt_caret);
      document_->editor_->setTextCursor(cursor);
    } catch (...) {
      document_->editor_->setPlainText(original_text);
      apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
      document_->editor_->setTextCursor(original_cursor);
      document_->editor_->document()->setModified(original_modified);
      throw;
    }

    document_->jwp_document_ = std::move(candidate);
    document_->jwp_history_ = std::move(history);
    document_->jwp_caret_ = following;
    document_->expected_jwp_caret_.reset();
    document_->rendered_jwp_text_ = std::move(rendered);
    document_->editor_->document()->setModified(
        !document_->saved_jwp_document_.has_value() ||
        document_->jwp_document_->document() != *document_->saved_jwp_document_);
    update_undo_actions();
    update_conversion_actions();
    update_title();
    statusBar()->showMessage(tr("Inserted dictionary result"), 2000);
    return true;
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not insert dictionary result: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

bool MainWindow::conversion_active() const noexcept {
  return document_->jwp_conversion_ != nullptr && document_->jwp_conversion_->active();
}

bool MainWindow::convert_selection() {
  if (conversion_active()) {
    return cycle_conversion();
  }
  if (!document_->jwp_document_.has_value() || wnn_resources_ == nullptr) {
    statusBar()->showMessage(tr("WNN conversion is not available"), 3000);
    return false;
  }
  try {
    apply_kana_input_events(document_->kana_input_.force_conversion());
    document_->kana_input_.discard();
    if (conversion_active()) return true;
    if (document_->automatic_conversion_range_.has_value()) {
      if (!document_->editor_->textCursor().hasSelection()) return attempt_automatic_conversion(true);
      clear_automatic_conversion_range();
    }
    const QTextCursor cursor = document_->editor_->textCursor();
    if (!cursor.hasSelection()) {
      statusBar()->showMessage(tr("Select kana to convert"), 3000);
      return false;
    }
    const QString text = document_plain_text(*document_->editor_->document());
    const core::JwpPosition begin = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(text, cursor.selectionStart()));
    const core::JwpPosition end = core::jwp_plain_text_position(
        *document_->jwp_document_, utf32_offset_for_utf16(text, cursor.selectionEnd()));
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *document_->jwp_document_, utf32_offset_for_utf16(text, cursor.position()));

    auto transaction = std::make_unique<core::JwpConversionTransaction>(
        *document_->jwp_document_, document_->jwp_history_, wnn_resources_->session);
    document_->conversion_preferences_before_ = wnn_resources_->preferences;
    if (!transaction->begin({begin, end}, caret)) {
      document_->conversion_preferences_before_.reset();
      statusBar()->showMessage(tr("No conversion candidates"), 3000);
      return false;
    }
    document_->jwp_conversion_ = std::move(transaction);
    document_->editor_->setReadOnly(true);
    restore_jwp_conversion_state();
    return true;
  } catch (const std::exception& error) {
    rollback_conversion_noexcept();
    statusBar()->showMessage(
        tr("Could not start conversion: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

bool MainWindow::cycle_conversion(bool previous) {
  if (!conversion_active()) {
    return false;
  }
  try {
    const bool changed = previous ? document_->jwp_conversion_->cycle_previous()
                                  : document_->jwp_conversion_->cycle_next();
    restore_jwp_conversion_state();
    return changed;
  } catch (const std::exception& error) {
    rollback_conversion_noexcept();
    statusBar()->showMessage(
        tr("Could not change candidate: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

bool MainWindow::accept_conversion() {
  if (!conversion_active()) {
    return false;
  }
  try {
    const core::JwpPosition caret = document_->jwp_conversion_->caret();
    document_->jwp_conversion_->accept();
    document_->jwp_conversion_.reset();
    document_->conversion_preferences_before_.reset();
    document_->editor_->set_transient_extra_selections({});
    document_->editor_->setReadOnly(false);
    restore_jwp_history_state(caret);
    try {
      save_wnn_preferences();
      statusBar()->showMessage(tr("Accepted conversion"), 3000);
    } catch (const std::exception& error) {
      statusBar()->showMessage(
          tr("Accepted conversion, but could not save preferences: %1")
              .arg(QString::fromUtf8(error.what())),
          5000);
    }
    update_conversion_actions();
    return true;
  } catch (const std::exception& error) {
    rollback_conversion_noexcept();
    statusBar()->showMessage(
        tr("Could not accept conversion: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

void MainWindow::restore_jwp_conversion_state() {
  if (!conversion_active() || !document_->jwp_document_.has_value()) {
    throw core::JwpConversionError("native conversion is not active");
  }
  std::u32string text =
      core::decode_jwp_plain_text(*document_->jwp_document_, document_->jwp_code_page_);
  const core::JwpRange range = document_->jwp_conversion_->range();
  const core::JwpPosition caret = document_->jwp_conversion_->caret();
  const int begin = utf16_offset_for_utf32(
      text, core::jwp_plain_text_offset(*document_->jwp_document_, range.begin));
  const int end = utf16_offset_for_utf32(
      text, core::jwp_plain_text_offset(*document_->jwp_document_, range.end));

  document_->updating_editor_ = true;
  document_->editor_->setPlainText(to_qstring(text));
  apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
  QTextCursor cursor = document_->editor_->textCursor();
  document_->editor_->set_transient_extra_selections({});
  if (caret == range.begin) {
    cursor.setPosition(end);
    cursor.setPosition(begin, QTextCursor::KeepAnchor);
  } else if (caret == range.end) {
    cursor.setPosition(begin);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
  } else {
    const int caret_offset = utf16_offset_for_utf32(
        text, core::jwp_plain_text_offset(*document_->jwp_document_, caret));
    cursor.setPosition(caret_offset);
    QTextEdit::ExtraSelection selection;
    QTextCursor selected(document_->editor_->document());
    selected.setPosition(begin);
    selected.setPosition(end, QTextCursor::KeepAnchor);
    selection.cursor = selected;
    selection.format.setBackground(
        document_->editor_->palette().brush(QPalette::Highlight));
    selection.format.setForeground(
        document_->editor_->palette().brush(QPalette::HighlightedText));
    document_->editor_->set_transient_extra_selections({selection});
  }
  document_->editor_->setTextCursor(cursor);
  document_->updating_editor_ = false;

  document_->rendered_jwp_text_ = std::move(text);
  document_->jwp_caret_ = caret;
  document_->expected_jwp_caret_.reset();
  document_->editor_->document()->setModified(
      !document_->saved_jwp_document_.has_value() ||
      document_->jwp_document_->document() != *document_->saved_jwp_document_);
  const std::size_t selected = document_->jwp_conversion_->selected_index() + 1U;
  const std::size_t total = document_->jwp_conversion_->result().candidates.size();
  {
    const QSignalBlocker blocker(conversion_candidates_);
    if (conversion_candidates_->count() == 0) {
      for (const auto& candidate : document_->jwp_conversion_->result().candidates) {
        conversion_candidates_->addItem(
            to_qstring(core::decode_jwp_text(candidate.text, document_->jwp_code_page_)));
      }
    }
    conversion_candidates_->setCurrentRow(static_cast<int>(selected - 1));
    conversion_candidates_->scrollToItem(conversion_candidates_->currentItem());
  }
  statusBar()->showMessage(
      tr("Conversion candidate %1 of %2")
          .arg(static_cast<qulonglong>(selected))
          .arg(static_cast<qulonglong>(total)));
  update_undo_actions();
  update_conversion_actions();
  update_title();
}

void MainWindow::rollback_conversion_noexcept() noexcept {
  std::optional<core::JwpPosition> caret;
  if (conversion_active()) {
    try {
      caret = document_->jwp_conversion_->rollback();
    } catch (...) {
    }
  }
  document_->jwp_conversion_.reset();
  if (document_->conversion_preferences_before_.has_value()) {
    wnn_resources_->preferences =
        std::move(*document_->conversion_preferences_before_);
    document_->conversion_preferences_before_.reset();
  }
  document_->editor_->setReadOnly(false);
  document_->editor_->set_transient_extra_selections({});
  if (caret.has_value() && document_->jwp_document_.has_value()) {
    try {
      restore_jwp_history_state(*caret);
    } catch (...) {
    }
  }
  update_undo_actions();
  update_conversion_actions();
}

void MainWindow::save_wnn_preferences() {
  if (wnn_resources_ != nullptr && wnn_resources_->preferences.changed()) {
    write_wnn_preferences_file(wnn_resources_->preferences_path,
                               wnn_resources_->preferences);
  }
}

void MainWindow::new_document() {
  if (!maybe_save()) {
    return;
  }
  core::JwpDocument document;
  document.margins.fill(1.0F);  // Recovered default_config: one-inch margins.
  document.paragraphs.emplace_back();
  load_jwp_document({}, std::move(document), document_->jwp_code_page_);
}

void MainWindow::open_document() {
  QString selected_filter = all_files_filter();
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open document"), QString(), file_filters() + QStringLiteral(";;") + project_filter(), &selected_filter);
  if (path.isEmpty()) {
    return;
  }
  if (selected_filter == project_filter() || (selected_filter == all_files_filter() &&
      QFileInfo(path).suffix().compare(QStringLiteral("jpr"), Qt::CaseInsensitive) == 0)) {
    open_project_dialog(path);
    return;
  }
  if (selected_filter == jwp_filter()) {
    open_jwp_path(path, default_jwp_code_page(), OpenMode::kInteractive, true);
    return;
  }
  const std::optional<core::TextEncoding> encoding =
      encoding_from_filter(selected_filter);
  if (encoding.has_value()) {
    open_path(path, *encoding, OpenMode::kInteractive, true);
  } else {
    open_path_detected(path, OpenMode::kInteractive, true);
  }
}

bool MainWindow::open_project_dialog(const QString& selected_path) {
  const auto path = selected_path.isEmpty() ? QFileDialog::getOpenFileName(
      this, tr("Open Project"), {}, project_filter()) : selected_path;
  if (path.isEmpty()) return false;
  ProjectOpenOptions options;
  const bool blank = document_count() == 1 && document_->current_path_.isEmpty() &&
      !document_modified() && !conversion_active() && !document_->kana_input_.pending() &&
      document_plain_text(*document_->editor_->document()).isEmpty();
  if (!blank) {
    const auto choice = QMessageBox::question(this, tr("Open Project"),
        tr("Replace the open workspace?\nYes: replace after resolving unsaved changes.\nNo: append and retain existing buffers."),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice == QMessageBox::Cancel) return false;
    options.append = choice == QMessageBox::No;
  }
  return open_project_path(path, options, OpenMode::kInteractive);
}

void MainWindow::save_project_dialog() {
  const auto path = QFileDialog::getSaveFileName(this, tr("Save Project"), project_path_, project_filter());
  if (path.isEmpty()) return;
  const auto choice = QMessageBox::question(this, tr("Save Project"),
      tr("Save document changes first?\nYes: save documents, then the project.\nNo: save file references only; unsaved content is not stored."),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
  if (choice != QMessageBox::Cancel) save_project_path(path, choice == QMessageBox::Yes, OpenMode::kInteractive);
}

int MainWindow::find_document_path(const QString& path) const {
  if (path.isEmpty()) return -1;
  const QString identity = document_path_identity(path);
  for (int i = 0; i < document_count(); ++i) {
    if (documents_[i]->current_path_.isEmpty()) continue;
    if (identity == document_path_identity(documents_[i]->current_path_)) return i;
  }
  return -1;
}

bool MainWindow::open_path(const QString& path, core::TextEncoding encoding,
                           OpenMode mode, bool new_tab) {
  const int existing = find_document_path(path);
  if (existing >= 0 && (new_tab || existing != current_document_index())) {
    const bool activated = activate_document(existing);
    if (activated) statusBar()->showMessage(tr("Already open: %1 (existing format retained)").arg(path), 3000);
    if (activated) record_recent_document(*document_);
    return activated;
  }
  if (!new_tab && conversion_active() && !accept_conversion()) {
    return false;
  }
  try {
    const core::TextFile file = read_text_file(path, encoding);
    load_document(path, file, true, new_tab);
    statusBar()->showMessage(
        tr("Opened %1 as %2").arg(path, encoding_name(document_->encoding_)), 3000);
    record_recent_document(*document_);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not open %1").arg(path), error);
    }
    return false;
  }
}

bool MainWindow::revert_current_document(OpenMode mode) {
  if (document_->current_path_.isEmpty() || conversion_active())
    return false;
  finish_kana_input();
  if (conversion_active())
    return false;
  if (mode == OpenMode::kInteractive && document_modified() &&
      !prompt_to_revert(document_->current_path_)) {
    return false;
  }
  const QString path = document_->current_path_;
  return document_->jwp_format_ ? open_jwp_path(path, document_->jwp_code_page_, mode)
                     : open_path(path, document_->encoding_, mode);
}

bool MainWindow::delete_current_document(OpenMode mode) {
  if (document_->current_path_.isEmpty() || conversion_active())
    return false;
  finish_kana_input();
  if (conversion_active())
    return false;
  const QString path = document_->current_path_;
  if (mode == OpenMode::kInteractive && !prompt_to_delete(path))
    return false;
  QFile file(path);
  if (!file.remove()) {
    const std::runtime_error error(
        tr("Could not remove %1: %2").arg(path, file.errorString())
            .toUtf8()
            .toStdString());
    if (mode == OpenMode::kInteractive)
      show_error(tr("Could not delete %1").arg(path), error);
    return false;
  }
  document_->editor_->document()->setModified(false);
  document_->saved_text_file_.reset();
  new_document();
  statusBar()->showMessage(tr("Deleted %1").arg(path), 3000);
  return true;
}

bool MainWindow::open_jwp_path(const QString& path,
                               core::LegacyCodePage code_page,
                               OpenMode mode, bool new_tab) {
  const int existing = find_document_path(path);
  if (existing >= 0 && (new_tab || existing != current_document_index())) {
    const bool activated = activate_document(existing);
    if (activated) statusBar()->showMessage(tr("Already open: %1 (existing format retained)").arg(path), 3000);
    if (activated) record_recent_document(*document_);
    return activated;
  }
  if (!new_tab && conversion_active() && !accept_conversion()) {
    return false;
  }
  try {
    load_jwp_document(path, read_jwp_file(path), code_page, new_tab);
    statusBar()->showMessage(
        tr("Opened %1 as JWP (%2)").arg(path, code_page_name(code_page)),
        3000);
    record_recent_document(*document_);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not open %1").arg(path), error);
    }
    return false;
  }
}

bool MainWindow::open_path_detected(const QString& path, OpenMode mode, bool new_tab) {
  const auto open_project = [&] {
    if (mode == OpenMode::kInteractive) return open_project_dialog(path);
    ProjectOpenOptions options;
    options.append = new_tab;
    return open_project_path(path, options, mode);
  };
  const int existing = find_document_path(path);
  if (existing >= 0 && (new_tab || existing != current_document_index())) {
    const bool activated = activate_document(existing);
    if (activated) statusBar()->showMessage(tr("Already open: %1 (existing format retained)").arg(path), 3000);
    if (activated) record_recent_document(*document_);
    return activated;
  }
  if (QFileInfo(path).suffix().compare(QStringLiteral("jpr"), Qt::CaseInsensitive) == 0)
    return open_project();
  if (!new_tab && conversion_active() && !accept_conversion()) {
    return false;
  }
  try {
    const std::string bytes = read_file_bytes(path);
    if (project_magic(bytes)) return open_project();
    if (core::has_jwp_document_magic(bytes)) {
      load_jwp_document(path, core::decode_jwp_document(bytes),
                        default_jwp_code_page(), new_tab);
      statusBar()->showMessage(
          tr("Opened %1 as JWP (%2)")
              .arg(path, code_page_name(document_->jwp_code_page_)),
          3000);
      record_recent_document(*document_);
      return true;
    }

    if (QFileInfo(path).suffix().compare(QStringLiteral("jfc"),
                                        Qt::CaseInsensitive) == 0) {
      load_document(path, core::decode_text_file(bytes, core::TextEncoding::kJfc),
                    true, new_tab);
      statusBar()->showMessage(
          tr("Opened %1 as %2").arg(path, encoding_name(document_->encoding_)), 3000);
      record_recent_document(*document_);
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
    load_document(path, file, true, new_tab);
    statusBar()->showMessage(
        tr("Opened %1 as %2").arg(path, encoding_name(*encoding)), 3000);
    record_recent_document(*document_);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not open %1").arg(path), error);
    }
    return false;
  }
}

void MainWindow::load_document(const QString& path,
                               const core::TextFile& file,
                               bool japanese_editing, bool new_tab) {
  QTextDocument staged_text;
  staged_text.setPlainText(to_qstring(file.text));
  core::TextFile normalized = file;
  normalized.text = from_qstring(document_plain_text(staged_text));
  std::optional<core::JwpDocumentModel> model;
  const auto code_page = new_tab ? default_jwp_code_page() : document_->jwp_code_page_;
  if (japanese_editing) {
    try {
      model = core::import_jwp_plain_text(normalized.text, code_page);
    } catch (const core::JwpPlainTextError&) {
      // Unrestricted Unicode stays editable; no replacement or truncation.
    }
  }
  if (model) {
    load_jwp_document(path, model->document(), code_page, new_tab);
    document_->jwp_format_ = false;
    document_->encoding_ = file.encoding;
    document_->has_byte_order_mark_ = file.has_byte_order_mark;
    document_->saved_text_file_ = std::move(normalized);
    update_encoding_display();
    return;
  }
  if (new_tab && new_document_tab(false) < 0)
    throw std::runtime_error("Could not finish input before opening a document");
  reset_kana_input(true);
  document_->jwp_document_.reset();
  document_->saved_jwp_document_.reset();
  document_->pristine_jwp_document_.reset();
  document_->jwp_history_.clear();
  document_->jwp_caret_.reset();
  document_->expected_jwp_caret_.reset();
  document_->rendered_jwp_text_.clear();
  document_->jwp_format_ = false;
  document_->current_path_ = path;
  document_->encoding_ = file.encoding;
  document_->has_byte_order_mark_ = file.has_byte_order_mark;
  document_->saved_text_file_ = std::move(normalized);
  {
    QScopedValueRollback<bool> guard(document_->updating_editor_, true);
    document_->editor_->setPlainText(document_plain_text(staged_text));
    clear_jwp_presentation();
    document_->editor_->document()->setModified(false);
  }
  update_encoding_display();
  update_undo_actions();
  update_conversion_actions();
  update_title();
}

void MainWindow::load_jwp_document(const QString& path,
                                      core::JwpDocument document,
                                      core::LegacyCodePage code_page, bool new_tab) {
  std::optional<core::JwpDocument> pristine_document;
  if (document.paragraphs.empty()) {
    pristine_document = document;
  }
  core::JwpDocumentModel model(std::move(document));
  std::u32string text = core::decode_jwp_plain_text(model, code_page);

  // Validate every presentation transformation before replacing the live
  // document. The live calls below then cannot fail on text/model mismatch.
  JwpEditor staged_editor;
  staged_editor.setFont(document_->editor_->font());
  staged_editor.setPlainText(to_qstring(text));
  staged_editor.apply_jwp_layout(model.document());
  staged_editor.prepare_kanji_colors(model.document(), kanji_color_list_,
                                      kanji_color_policy_, code_page);

  if (new_tab && new_document_tab() < 0)
    throw std::runtime_error("Could not finish input before opening a document");
  reset_kana_input(false);
  {
    QScopedValueRollback<bool> update_guard(document_->updating_editor_, true);
    document_->editor_->setPlainText(to_qstring(text));
    apply_jwp_presentation(model.document(), code_page);
  }
  document_->jwp_document_ = std::move(model);
  document_->saved_jwp_document_ = document_->jwp_document_->document();
  document_->pristine_jwp_document_ = std::move(pristine_document);
  document_->jwp_history_.clear();
  document_->jwp_caret_ = core::JwpPosition{};
  document_->expected_jwp_caret_.reset();
  document_->rendered_jwp_text_ = std::move(text);
  document_->jwp_code_page_ = code_page;
  document_->current_path_ = path;
  document_->has_byte_order_mark_ = false;
  document_->jwp_format_ = true;
  document_->saved_text_file_.reset();
  document_->editor_->document()->setModified(false);
  update_encoding_display();
  update_undo_actions();
  update_conversion_actions();
  update_title();
}

bool MainWindow::save_document() {
  if (document_->current_path_.isEmpty()) return save_document_as();
  if (!document_->jwp_format_ && !confirm_text_export()) return false;
  return save_as_path(document_->current_path_, document_->jwp_format_ ? std::nullopt
                                               : std::optional{document_->encoding_},
                      true, false, OpenMode::kInteractive);
}

bool MainWindow::confirm_text_export() {
  if (!document_->jwp_document_) return true;
  const auto report = core::export_jwp_plain_text(*document_->jwp_document_, document_->jwp_code_page_);
  if (report.lossless()) return true;
  QStringList losses;
  if (report.loses_formatting) losses << tr("paragraph and page layout");
  if (report.loses_metadata) losses << tr("headers, footers and summary metadata");
  if (report.loses_page_breaks) losses << tr("hard page breaks");
  return QMessageBox::warning(
      this, tr("Text format loses document information"),
      tr("The text file will not preserve %1. Continue?")
          .arg(losses.join(QStringLiteral(", "))),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) ==
      QMessageBox::Yes;
}

bool MainWindow::save_document_as(bool export_copy) {
  if (export_copy && (conversion_active() || document_->kana_input_.pending())) {
    statusBar()->showMessage(tr("Finish input or conversion before exporting a copy"), 5000);
    return false;
  }
  QString selected_filter = document_->jwp_format_ ? jwp_filter() : encoding_filter(document_->encoding_);
  const QString path = QFileDialog::getSaveFileName(
      this, export_copy ? tr("Export document copy") : tr("Save document"),
      export_copy ? QString() : document_->current_path_, file_filters(),
      &selected_filter);
  if (path.isEmpty()) {
    return false;
  }
  std::optional<core::TextEncoding> encoding;
  if (selected_filter != jwp_filter()) {
    encoding = encoding_from_filter(selected_filter);
    if (!encoding) encoding = choose_encoding();
    if (!encoding || !confirm_text_export()) return false;
  }
  return save_as_path(path, encoding, true, export_copy, OpenMode::kInteractive);
}

bool MainWindow::save_path(const QString& path) {
  return save_as_path(path, document_->jwp_format_ ? std::nullopt : std::optional{document_->encoding_},
                      false, false, OpenMode::kInteractive);
}

bool MainWindow::save_as_path(const QString& path,
                              std::optional<core::TextEncoding> encoding,
                              bool allow_format_loss, bool export_copy,
                              OpenMode mode) {
  const int existing = find_document_path(path);
  if (existing >= 0 && documents_[existing].get() != document_) {
    if (mode == OpenMode::kInteractive)
      QMessageBox::warning(this, tr("Document is already open"),
                           tr("This path belongs to another open document."));
    return false;
  }
  if (export_copy) {
    if (conversion_active() || document_->kana_input_.pending()) return false;
    if (!document_->current_path_.isEmpty() &&
        document_path_identity(path) == document_path_identity(document_->current_path_)) {
      return false;
    }
  } else {
    if (conversion_active() && !accept_conversion()) return false;
    finish_kana_input();
  }
  try {
    std::optional<core::TextFile> text_file;
    std::optional<core::JwpDocument> saved_document;
    if (!export_copy && document_->jwp_document_) saved_document = document_->jwp_document_->document();
    if (!encoding) {
      const bool unedited_pristine = document_->jwp_document_ &&
          document_->pristine_jwp_document_.has_value() &&
          document_->saved_jwp_document_.has_value() &&
          document_->jwp_document_->document() == *document_->saved_jwp_document_;
      if (document_->jwp_document_) {
        write_jwp_file(path, unedited_pristine ? *document_->pristine_jwp_document_
                                             : document_->jwp_document_->document());
      } else {
        const auto model = core::import_jwp_plain_text(
            from_qstring(document_plain_text(*document_->editor_->document())), document_->jwp_code_page_);
        if (!export_copy) saved_document = model.document();
        write_jwp_file(path, model.document());
      }
    } else {
      auto report = document_->jwp_document_ ? core::export_jwp_plain_text(*document_->jwp_document_, document_->jwp_code_page_)
                                  : core::JwpPlainTextExport{from_qstring(document_plain_text(*document_->editor_->document()))};
      if (!allow_format_loss && !report.lossless()) {
        throw std::runtime_error("Text export would lose formatting, metadata or page breaks; explicit approval is required");
      }
      bool bom = !document_->jwp_format_ && *encoding == document_->encoding_ ? document_->has_byte_order_mark_ :
          *encoding == core::TextEncoding::kUtf16Le ||
          *encoding == core::TextEncoding::kUtf16Be ||
          (*encoding == core::TextEncoding::kUtf8 && document_->has_byte_order_mark_);
      const bool utf16 = *encoding == core::TextEncoding::kUtf16Le ||
                         *encoding == core::TextEncoding::kUtf16Be;
      if (!report.text.empty() &&
          ((report.text.front() == U'\ufeff' &&
            (*encoding == core::TextEncoding::kUtf8 || utf16)) ||
           (report.text.front() == U'\ufffe' && utf16))) {
        // Distinguish a leading text character from the file's signature.
        bom = true;
      }
      text_file = core::TextFile{std::move(report.text), *encoding, bom};
      write_text_file(path, *text_file);
    }
    if (!export_copy) {
      if (document_->pristine_jwp_document_ && document_->jwp_document_ &&
          (!document_->saved_jwp_document_ || document_->jwp_document_->document() != *document_->saved_jwp_document_)) {
        document_->pristine_jwp_document_.reset();
      }
      document_->saved_jwp_document_ = std::move(saved_document);
      document_->jwp_format_ = !encoding.has_value();
      if (text_file) {
        document_->encoding_ = text_file->encoding;
        document_->has_byte_order_mark_ = text_file->has_byte_order_mark;
      }
      document_->saved_text_file_ = std::move(text_file);
      document_->current_path_ = path;
      document_->editor_->document()->setModified(false);
      update_encoding_display();
      update_title();
    }
    statusBar()->showMessage(
        (export_copy ? tr("Exported %1 as %2") : tr("Saved %1 as %2"))
            .arg(path, encoding ? encoding_name(*encoding) : tr("JWP")), 3000);
    if (!export_copy) record_recent_document(*document_);
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) {
      show_error(tr("Could not save %1").arg(path), error);
    }
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
    if (encoding == document_->encoding_) {
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

std::optional<core::JwpParagraphFormat>
MainWindow::prompt_for_paragraph_format(
    const core::JwpParagraphFormat& initial) {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Paragraph Format"));

  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* left = new QSpinBox(&dialog);
  left->setObjectName(QStringLiteral("paragraphLeftIndent"));
  left->setRange(0, 255);
  left->setValue(initial.left_indent);
  form->addRow(tr("Left indent (characters):"), left);

  auto* right = new QSpinBox(&dialog);
  right->setObjectName(QStringLiteral("paragraphRightIndent"));
  right->setRange(0, 255);
  right->setValue(initial.right_indent);
  form->addRow(tr("Right indent (characters):"), right);

  auto* first = new QSpinBox(&dialog);
  first->setObjectName(QStringLiteral("paragraphFirstIndent"));
  first->setRange(-127, 127);
  first->setValue(initial.first_indent);
  form->addRow(tr("First-line indent (characters):"), first);

  auto* spacing = new QDoubleSpinBox(&dialog);
  spacing->setObjectName(QStringLiteral("paragraphLineSpacing"));
  spacing->setRange(1.0, 10.0);
  spacing->setDecimals(2);
  spacing->setSingleStep(0.05);
  spacing->setValue(static_cast<double>(initial.line_spacing) / 100.0);
  form->addRow(tr("Line spacing:"), spacing);
  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return core::JwpParagraphFormat{
      left->value(), right->value(), first->value(),
      static_cast<int>(spacing->value() * 100.0 + 0.5)};
}

std::optional<core::JwpDocument> MainWindow::prompt_for_page_layout(
    const core::JwpDocument& initial) {
  PageLayoutDialog dialog(initial, document_->jwp_code_page_, this);
  if (dialog.exec() != QDialog::Accepted)
    return std::nullopt;
  return dialog.document();
}

bool MainWindow::prompt_for_print(QPrinter& printer) {
  QPrintDialog dialog(&printer, this);
  dialog.setOption(QAbstractPrintDialog::PrintSelection,
                   document_->editor_->textCursor().hasSelection());
  return dialog.exec() == QDialog::Accepted;
}

bool MainWindow::prompt_for_printer_setup(QPrinter& printer) {
  QPageSetupDialog dialog(&printer, this);
  return dialog.exec() == QDialog::Accepted;
}

std::optional<core::KanjiColorPolicy>
MainWindow::prompt_for_kanji_color_policy(
    const core::KanjiColorPolicy& initial) {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Kanji Color Options"));

  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* mode = new QComboBox(&dialog);
  mode->setObjectName(QStringLiteral("kanjiColorMode"));
  mode->addItem(tr("Off"),
                static_cast<int>(core::KanjiListColorMode::kOff));
  mode->addItem(tr("Color kanji in the list"),
                static_cast<int>(core::KanjiListColorMode::kMatch));
  mode->addItem(tr("Color kanji not in the list"),
                static_cast<int>(core::KanjiListColorMode::kNoMatch));
  const int initial_mode = mode->findData(static_cast<int>(initial.list_mode));
  if (initial_mode < 0) {
    throw std::invalid_argument("Invalid kanji list color mode");
  }
  mode->setCurrentIndex(initial_mode);
  form->addRow(tr("List coloring:"), mode);

  QColor list_color(initial.list_color.red, initial.list_color.green,
                    initial.list_color.blue);
  auto* list_color_button = new QPushButton(list_color.name(), &dialog);
  list_color_button->setObjectName(QStringLiteral("kanjiListColor"));
  connect(list_color_button, &QPushButton::clicked, &dialog, [&] {
    const QColor selected = QColorDialog::getColor(
        list_color, &dialog, tr("Select Kanji List Color"));
    if (selected.isValid()) {
      list_color = selected;
      list_color_button->setText(list_color.name());
    }
  });
  form->addRow(tr("List color:"), list_color_button);

  auto* uncommon = new QCheckBox(tr("Color uncommon kanji"), &dialog);
  uncommon->setObjectName(QStringLiteral("kanjiUncommonEnabled"));
  uncommon->setChecked(initial.colorize_uncommon);
  form->addRow(QString(), uncommon);

  QColor uncommon_color(initial.uncommon_color.red,
                        initial.uncommon_color.green,
                        initial.uncommon_color.blue);
  auto* uncommon_color_button =
      new QPushButton(uncommon_color.name(), &dialog);
  uncommon_color_button->setObjectName(
      QStringLiteral("kanjiUncommonColor"));
  connect(uncommon_color_button, &QPushButton::clicked, &dialog, [&] {
    const QColor selected = QColorDialog::getColor(
        uncommon_color, &dialog, tr("Select Uncommon Kanji Color"));
    if (selected.isValid()) {
      uncommon_color = selected;
      uncommon_color_button->setText(uncommon_color.name());
    }
  });
  form->addRow(tr("Uncommon color:"), uncommon_color_button);
  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return core::KanjiColorPolicy{
      static_cast<core::KanjiListColorMode>(mode->currentData().toInt()),
      {static_cast<std::uint8_t>(list_color.red()),
       static_cast<std::uint8_t>(list_color.green()),
       static_cast<std::uint8_t>(list_color.blue())},
      uncommon->isChecked(),
      {static_cast<std::uint8_t>(uncommon_color.red()),
       static_cast<std::uint8_t>(uncommon_color.green()),
       static_cast<std::uint8_t>(uncommon_color.blue())}};
}

std::optional<KanjiColorListEditRequest>
MainWindow::prompt_for_kanji_color_list_edit() {
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Add or Remove Kanji"));

  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout();
  auto* operation = new QComboBox(&dialog);
  operation->setObjectName(QStringLiteral("kanjiColorListOperation"));
  operation->addItem(tr("Add to list"), true);
  operation->addItem(tr("Remove from list"), false);
  form->addRow(tr("Operation:"), operation);
  auto* text = new QLineEdit(&dialog);
  text->setObjectName(QStringLiteral("kanjiColorListText"));
  form->addRow(tr("Kanji:"), text);
  layout->addLayout(form);

  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return KanjiColorListEditRequest{text->text(),
                                   operation->currentData().toBool()};
}

void MainWindow::set_text_encoding(core::TextEncoding encoding,
                                    bool mark_modified) {
  if (document_->jwp_format_ || document_->encoding_ == encoding) {
    return;
  }
  document_->encoding_ = encoding;
  if (document_->encoding_ == core::TextEncoding::kUtf16Le ||
      document_->encoding_ == core::TextEncoding::kUtf16Be) {
    document_->has_byte_order_mark_ = true;
  } else if (document_->encoding_ != core::TextEncoding::kUtf8) {
    document_->has_byte_order_mark_ = false;
  }
  update_encoding_display();
  if (mark_modified) {
    document_->editor_->document()->setModified(document_->jwp_document_ ? native_document_modified() :
        !document_->saved_text_file_ || document_plain_text(*document_->editor_->document()) != to_qstring(document_->saved_text_file_->text) ||
        document_->encoding_ != document_->saved_text_file_->encoding ||
        document_->has_byte_order_mark_ != document_->saved_text_file_->has_byte_order_mark);
  }
  update_title();
}

void MainWindow::set_jwp_code_page(core::LegacyCodePage code_page) {
  finish_kana_input();
  if (conversion_active() && !accept_conversion()) {
    return;
  }
  if (document_->jwp_code_page_ == code_page) {
    return;
  }
  if (!document_->jwp_document_.has_value()) {
    document_->jwp_code_page_ = code_page;
    update_encoding_display();
    return;
  }
  try {
    std::u32string text =
        core::decode_jwp_plain_text(*document_->jwp_document_, code_page);
    const bool modified = document_->saved_jwp_document_.has_value() &&
                          document_->jwp_document_->document() != *document_->saved_jwp_document_;
    document_->updating_editor_ = true;
    document_->editor_->setPlainText(to_qstring(text));
    apply_jwp_presentation(document_->jwp_document_->document(), code_page);
    document_->updating_editor_ = false;
    document_->rendered_jwp_text_ = std::move(text);
    document_->jwp_code_page_ = code_page;
    document_->editor_->document()->setModified(modified);
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

void MainWindow::format_document_paragraphs() {
  finish_kana_input();
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return;
  }
  try {
    const QString text = document_plain_text(*document_->editor_->document());
    const QTextCursor cursor = document_->editor_->textCursor();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(text, cursor.position()));
    const std::optional<core::JwpParagraphFormat> format =
        prompt_for_paragraph_format(
            document_->jwp_document_->paragraph_format(caret.paragraph));
    if (format.has_value()) {
      format_paragraphs(*format);
    }
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not format paragraphs: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
  }
}

void MainWindow::format_file_paragraphs() {
  finish_kana_input();
  if (conversion_active() || !document_->jwp_document_.has_value() ||
      document_->jwp_document_->paragraph_count() == 0) {
    return;
  }
  try {
    const QString text = document_plain_text(*document_->editor_->document());
    const QTextCursor cursor = document_->editor_->textCursor();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *document_->jwp_document_, utf32_offset_for_utf16(text, cursor.position()));
    const std::optional<core::JwpParagraphFormat> format =
        prompt_for_paragraph_format(
            document_->jwp_document_->paragraph_format(caret.paragraph));
    if (format.has_value() &&
        apply_paragraph_format(0, document_->jwp_document_->paragraph_count() - 1,
                               *format, cursor, caret)) {
      statusBar()->showMessage(tr("File format applied"), 2000);
    }
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not format file: %1").arg(QString::fromUtf8(error.what())),
        5000);
  }
}

void MainWindow::format_page_layout() {
  finish_kana_input();
  if (conversion_active() || !document_->jwp_document_.has_value())
    return;
  try {
    const std::optional<core::JwpDocument> requested =
        prompt_for_page_layout(document_->jwp_document_->document());
    if (requested.has_value() && apply_page_layout(*requested))
      statusBar()->showMessage(tr("Page layout applied"), 2000);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not apply page layout: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
  }
}

bool MainWindow::apply_page_layout(const core::JwpDocument& requested) {
  if (!document_->jwp_document_.has_value() || requested == document_->jwp_document_->document())
    return false;

  const QTextCursor cursor = document_->editor_->textCursor();
  const core::JwpPosition caret = core::jwp_plain_text_position(
      *document_->jwp_document_,
      utf32_offset_for_utf16(document_plain_text(*document_->editor_->document()), cursor.position()));
  core::JwpDocumentModel candidate(requested);
  core::JwpDocumentHistory history = document_->jwp_history_;
  history.begin(*document_->jwp_document_, caret);
  if (!history.commit(candidate, caret))
    return false;

  apply_jwp_presentation(candidate.document(), document_->jwp_code_page_);
  document_->jwp_document_ = std::move(candidate);
  document_->jwp_history_ = std::move(history);
  document_->jwp_caret_ = caret;
  document_->expected_jwp_caret_.reset();
  document_->editor_->setTextCursor(cursor);
  document_->editor_->document()->setModified(
      !document_->saved_jwp_document_.has_value() ||
      document_->jwp_document_->document() != *document_->saved_jwp_document_);
  update_undo_actions();
  update_title();
  return true;
}

void MainWindow::print_current_document() {
  finish_kana_input();
  if (conversion_active())
    return;
  try {
    const core::JwpDocument* jwp =
        document_->jwp_document_.has_value() ? &document_->jwp_document_->document() : nullptr;
    if (jwp != nullptr)
      configure_printer_for_jwp(*printer_, *jwp);
    if (!prompt_for_print(*printer_))
      return;
    print_document(*printer_, *document_->editor_->document(), jwp);
    statusBar()->showMessage(tr("Document sent to printer"), 3000);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not print document: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
  }
}

void MainWindow::setup_printer() {
  finish_kana_input();
  if (conversion_active())
    return;
  try {
    if (!prompt_for_printer_setup(*printer_))
      return;
    if (document_->jwp_document_.has_value()) {
      core::JwpDocument candidate = document_->jwp_document_->document();
      candidate.landscape =
          printer_->pageLayout().orientation() == QPageLayout::Landscape;
      (void)apply_page_layout(candidate);
    }
    statusBar()->showMessage(tr("Printer setup updated"), 2000);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not configure printer: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
  }
}

void MainWindow::configure_kanji_colors() {
  try {
    const std::optional<core::KanjiColorPolicy> policy =
        prompt_for_kanji_color_policy(kanji_color_policy_);
    if (policy.has_value()) {
      set_kanji_color_policy(*policy);
    }
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not configure kanji colors: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
  }
}

void MainWindow::edit_kanji_color_list() {
  try {
    const std::optional<KanjiColorListEditRequest> request =
        prompt_for_kanji_color_list_edit();
    if (request.has_value()) {
      edit_kanji_color_list(request->text, request->add);
    }
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not edit kanji color list: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
  }
}

bool MainWindow::find_text(const QString& text,
                           core::JwpSearchOptions options) {
  finish_kana_input();
  if (conversion_active()) {
    statusBar()->showMessage(tr("Accept the current conversion before finding"),
                             3000);
    return false;
  }
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
      core::encode_jwp_text(from_qstring(text), document_->jwp_code_page_);
  const QTextCursor original = document_->editor_->textCursor();
  const int start_utf16 = original.hasSelection() ? original.selectionStart()
                                                  : original.position();
  const std::size_t start_offset =
      utf32_offset_for_utf16(document_plain_text(*document_->editor_->document()), start_utf16);
  const core::JwpSearchResult result = core::find_next(
      *document_->jwp_document_, pattern,
      core::jwp_plain_text_position(*document_->jwp_document_, start_offset), options);
  if (!result.match.has_value()) {
    statusBar()->showMessage(tr("Text not found"), 3000);
    return false;
  }

  const std::size_t begin =
      core::jwp_plain_text_offset(*document_->jwp_document_, result.match->begin);
  const std::size_t end =
      core::jwp_plain_text_offset(*document_->jwp_document_, result.match->end);
  QTextCursor found = document_->editor_->textCursor();
  found.setPosition(utf16_offset_for_utf32(document_->rendered_jwp_text_, begin));
  found.setPosition(utf16_offset_for_utf32(document_->rendered_jwp_text_, end),
                    QTextCursor::KeepAnchor);
  document_->editor_->setTextCursor(found);
  statusBar()->showMessage(result.wrapped ? tr("Search wrapped")
                                         : tr("Match found"),
                           2000);
  return true;
}

bool MainWindow::find_plain_text(const QString& text,
                                 core::JwpSearchOptions options) {
  const QTextCursor original = document_->editor_->textCursor();
  const QString source = options.ignore_ascii_case
                             ? fold_ascii_case(document_plain_text(*document_->editor_->document()))
                             : document_plain_text(*document_->editor_->document());
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
    document_->editor_->setTextCursor(original);
    statusBar()->showMessage(tr("Text not found"), 3000);
    return false;
  }

  QTextCursor found = original;
  found.setPosition(static_cast<int>(match));
  found.setPosition(static_cast<int>(match + text.size()),
                    QTextCursor::KeepAnchor);
  document_->editor_->setTextCursor(found);
  statusBar()->showMessage(wrapped ? tr("Search wrapped")
                                   : tr("Match found"),
                           2000);
  return true;
}

bool MainWindow::replace_next(const QString& text,
                              const QString& replacement,
                              core::JwpSearchOptions options) {
  finish_kana_input();
  if (conversion_active()) {
    statusBar()->showMessage(
        tr("Accept the current conversion before replacing"), 3000);
    return false;
  }
  replacement_text_ = replacement;
  if (text.isEmpty()) {
    statusBar()->showMessage(tr("Enter text to replace"), 3000);
    return false;
  }

  const QTextCursor original = document_->editor_->textCursor();
  try {
    if (is_jwp_document()) {
      core::JwpDocumentModel validation;
      core::replace_jwp_plain_text(validation, 0, 0,
                                   from_qstring(replacement), document_->jwp_code_page_);
    }
    if (!find_text(text, options)) {
      return false;
    }
    QTextCursor match = document_->editor_->textCursor();
    std::optional<core::JwpDocument> expected_jwp;
    if (is_jwp_document()) {
      const QString current = document_plain_text(*document_->editor_->document());
      const std::size_t begin =
          utf32_offset_for_utf16(current, match.selectionStart());
      const std::size_t end =
          utf32_offset_for_utf16(current, match.selectionEnd());
      core::JwpDocumentModel candidate(document_->jwp_document_->document());
      core::replace_jwp_plain_text(candidate, begin, end - begin,
                                   from_qstring(replacement), document_->jwp_code_page_);
      expected_jwp = candidate.document();
    }

    match.insertText(replacement);
    if (expected_jwp.has_value() &&
        (!document_->jwp_document_.has_value() ||
         document_->jwp_document_->document() != *expected_jwp)) {
      throw core::JwpPlainTextError(
          "Native editor did not apply the validated JWP replacement");
    }
    statusBar()->showMessage(tr("Replaced one match"), 2000);
    return true;
  } catch (const std::exception& error) {
    document_->editor_->setTextCursor(original);
    statusBar()->showMessage(
        tr("Could not replace: %1").arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

std::size_t MainWindow::replace_all(const QString& text,
                                    const QString& replacement,
                                    core::JwpSearchOptions options) {
  finish_kana_input();
  if (conversion_active()) {
    statusBar()->showMessage(
        tr("Accept the current conversion before replacing"), 3000);
    return 0;
  }
  replacement_text_ = replacement;
  if (text.isEmpty()) {
    statusBar()->showMessage(tr("Enter text to replace"), 3000);
    return 0;
  }

  try {
    std::vector<std::pair<int, int>> matches;
    std::optional<core::JwpDocumentModel> candidate_jwp;
    std::optional<core::JwpDocumentHistory> candidate_history;
    std::optional<core::JwpPosition> candidate_caret;
    std::u32string expected_jwp_text;
    if (is_jwp_document()) {
      const core::JwpText pattern =
          core::encode_jwp_text(from_qstring(text), document_->jwp_code_page_);
      const std::u32string replacement_text = from_qstring(replacement);
      const std::vector<core::JwpRange> ranges =
          core::find_all(*document_->jwp_document_, pattern, options);
      candidate_jwp.emplace(document_->jwp_document_->document());
      candidate_history.emplace(document_->jwp_history_);
      std::size_t previous_original_end = 0;
      std::size_t current_offset = 0;
      for (const core::JwpRange& range : ranges) {
        const std::size_t begin =
            core::jwp_plain_text_offset(*document_->jwp_document_, range.begin);
        const std::size_t end =
            core::jwp_plain_text_offset(*document_->jwp_document_, range.end);
        current_offset += begin - previous_original_end;
        const core::JwpPosition before = core::jwp_plain_text_position(
            *candidate_jwp, current_offset);
        candidate_history->begin(*candidate_jwp, before);
        candidate_caret = core::replace_jwp_plain_text(
            *candidate_jwp, current_offset, end - begin, replacement_text,
            document_->jwp_code_page_);
        candidate_history->commit(*candidate_jwp, *candidate_caret);
        current_offset =
            core::jwp_plain_text_offset(*candidate_jwp, *candidate_caret);
        previous_original_end = end;
        matches.emplace_back(
            utf16_offset_for_utf32(document_->rendered_jwp_text_, begin),
            utf16_offset_for_utf32(document_->rendered_jwp_text_, end));
      }
      std::reverse(matches.begin(), matches.end());
      expected_jwp_text =
          core::decode_jwp_plain_text(*candidate_jwp, document_->jwp_code_page_);
    } else {
      const QString source = options.ignore_ascii_case
                                 ? fold_ascii_case(document_plain_text(*document_->editor_->document()))
                                 : document_plain_text(*document_->editor_->document());
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
    const QString original_text = document_plain_text(*document_->editor_->document());
    const QTextCursor original_cursor = document_->editor_->textCursor();
    const bool original_modified = document_->editor_->document()->isModified();
    {
      QScopedValueRollback<bool> update_guard(document_->updating_editor_,
                                               candidate_jwp.has_value());
      QTextCursor edit(document_->editor_->document());
      for (const auto& [begin, end] : matches) {
        edit.setPosition(begin);
        edit.setPosition(end, QTextCursor::KeepAnchor);
        edit.insertText(replacement);
      }
      if (candidate_jwp.has_value() &&
          document_plain_text(*document_->editor_->document()) != to_qstring(expected_jwp_text)) {
        document_->editor_->setPlainText(original_text);
        apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
        document_->editor_->setTextCursor(original_cursor);
        document_->editor_->document()->setModified(original_modified);
        throw core::JwpPlainTextError(
            "Native editor did not apply the validated JWP replacements");
      }
      if (candidate_jwp.has_value() && candidate_caret.has_value()) {
        const std::size_t caret_offset = core::jwp_plain_text_offset(
            *candidate_jwp, *candidate_caret);
        QTextCursor caret(document_->editor_->document());
        caret.setPosition(
            utf16_offset_for_utf32(expected_jwp_text, caret_offset));
        document_->editor_->setTextCursor(caret);
      }
    }
    if (candidate_jwp.has_value()) {
      const bool modified = document_->saved_jwp_document_.has_value() &&
                            candidate_jwp->document() != *document_->saved_jwp_document_;
      document_->editor_->apply_kanji_colors(candidate_jwp->document(),
                                  kanji_color_list_, kanji_color_policy_,
                                  document_->jwp_code_page_);
      document_->jwp_document_.emplace(std::move(*candidate_jwp));
      document_->jwp_history_ = std::move(*candidate_history);
      document_->jwp_caret_ = candidate_caret;
      document_->expected_jwp_caret_.reset();
      document_->rendered_jwp_text_ = std::move(expected_jwp_text);
      document_->editor_->document()->setModified(modified);
      update_undo_actions();
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

bool MainWindow::format_paragraphs(
    const core::JwpParagraphFormat& format) {
  finish_kana_input();
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return false;
  }

  try {
    const QString text = document_plain_text(*document_->editor_->document());
    const QTextCursor cursor = document_->editor_->textCursor();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(text, cursor.position()));
    const core::JwpPosition selection_begin = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(text, cursor.selectionStart()));
    const core::JwpPosition selection_end = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(text, cursor.selectionEnd()));

    return apply_paragraph_format(selection_begin.paragraph,
                                  selection_end.paragraph, format, cursor,
                                  caret);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not format paragraphs: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

bool MainWindow::apply_paragraph_format(
    std::size_t first_paragraph, std::size_t last_paragraph,
    const core::JwpParagraphFormat& format, const QTextCursor& cursor,
    core::JwpPosition caret) {
  core::JwpDocumentModel candidate = *document_->jwp_document_;
  core::JwpDocumentHistory history = document_->jwp_history_;
  history.begin(candidate, caret);
  candidate.format_paragraphs(first_paragraph, last_paragraph, format);
  if (!history.commit(candidate, caret))
    return true;

  const int page_width = document_->editor_->character_page_width();
  if (page_width > 0 &&
      (format.left_indent + format.right_indent >= page_width ||
       format.left_indent + format.right_indent + format.first_indent >=
           page_width)) {
    throw core::JwpDocumentEditError(
        "paragraph indents leave no usable line width");
  }

  apply_jwp_presentation(candidate.document(), document_->jwp_code_page_);
  document_->jwp_document_ = std::move(candidate);
  document_->jwp_history_ = std::move(history);
  document_->jwp_caret_ = caret;
  document_->expected_jwp_caret_.reset();
  document_->editor_->setTextCursor(cursor);
  const bool modified = !document_->saved_jwp_document_.has_value() ||
                        document_->jwp_document_->document() != *document_->saved_jwp_document_;
  document_->editor_->document()->setModified(modified);
  update_undo_actions();
  update_title();
  statusBar()->showMessage(tr("Paragraph format applied"), 2000);
  return true;
}

bool MainWindow::insert_page_break() {
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !document_->jwp_document_.has_value()) {
    return false;
  }

  try {
    const QString original_text = document_plain_text(*document_->editor_->document());
    const QTextCursor original_cursor = document_->editor_->textCursor();
    const bool original_modified = document_->editor_->document()->isModified();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *document_->jwp_document_,
        utf32_offset_for_utf16(original_text, original_cursor.position()));
    const core::JwpPosition selection_begin = core::jwp_plain_text_position(
        *document_->jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionStart()));
    const core::JwpPosition selection_end = core::jwp_plain_text_position(
        *document_->jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionEnd()));

    core::JwpDocumentModel candidate = *document_->jwp_document_;
    core::JwpDocumentHistory history = document_->jwp_history_;
    history.begin(candidate, caret);
    const core::JwpPosition insertion =
        candidate.erase({selection_begin, selection_end});
    const core::JwpPosition following = candidate.insert_page_break(insertion);
    if (!history.commit(candidate, following)) {
      throw core::JwpDocumentEditError(
          "page-break insertion did not change the document");
    }

    std::u32string rendered =
        core::decode_jwp_plain_text(candidate, document_->jwp_code_page_);
    const QString qt_text = to_qstring(rendered);
    const int qt_caret = utf16_offset_for_utf32(
        rendered, core::jwp_plain_text_offset(candidate, following));

    document_->updating_editor_ = true;
    try {
      document_->editor_->setPlainText(qt_text);
      apply_jwp_presentation(candidate.document(), document_->jwp_code_page_);
      QTextCursor cursor(document_->editor_->document());
      cursor.setPosition(qt_caret);
      document_->editor_->setTextCursor(cursor);
    } catch (...) {
      document_->editor_->setPlainText(original_text);
      apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
      document_->editor_->setTextCursor(original_cursor);
      document_->editor_->document()->setModified(original_modified);
      document_->updating_editor_ = false;
      throw;
    }
    document_->updating_editor_ = false;

    document_->jwp_document_ = std::move(candidate);
    document_->jwp_history_ = std::move(history);
    document_->jwp_caret_ = following;
    document_->expected_jwp_caret_.reset();
    document_->rendered_jwp_text_ = std::move(rendered);
    const bool modified = !document_->saved_jwp_document_.has_value() ||
                          document_->jwp_document_->document() != *document_->saved_jwp_document_;
    document_->editor_->document()->setModified(modified);
    update_undo_actions();
    update_conversion_actions();
    update_title();
    statusBar()->showMessage(tr("Page break inserted"), 2000);
    return true;
  } catch (const std::exception& error) {
    document_->updating_editor_ = false;
    statusBar()->showMessage(
        tr("Could not insert page break: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

void MainWindow::synchronize_jwp_document(int position, int chars_removed,
                                           int chars_added) {
  if (document_->updating_editor_ || !document_->jwp_document_.has_value()) {
    return;
  }

  if (document_->automatic_conversion_range_.has_value() && !document_->applying_kana_input_) {
    clear_automatic_conversion_range();
  }

  const int cursor_position = document_->editor_->textCursor().position();
  int rejected_selection_start = -1;
  int rejected_selection_end = -1;
  try {
    const QString current_qt = document_plain_text(*document_->editor_->document());
    std::u32string current = from_qstring(current_qt);
    if (current == document_->rendered_jwp_text_) {
      return;
    }

    // Clipboard insertion can replace Qt's final paragraph marker, which is
    // absent from both plain-text snapshots. Only discard a paired marker.
    if (chars_removed > 0 && chars_added > 0 &&
        static_cast<qsizetype>(position) + chars_removed ==
            to_qstring(document_->rendered_jwp_text_).size() + 1 &&
        static_cast<qsizetype>(position) + chars_added == current_qt.size() + 1) {
      --chars_removed;
      --chars_added;
    }
    const std::size_t prefix = utf32_offset_for_utf16(current_qt, position);
    const std::size_t replacement_end =
        utf32_offset_for_utf16(current_qt, position + chars_added);
    const std::size_t removed_length =
        static_cast<std::size_t>(chars_removed);
    const std::size_t replacement_length = replacement_end - prefix;
    current = core::replace_plain_text_snapshot(
        document_->rendered_jwp_text_, current, prefix, removed_length,
        std::u32string_view(current).substr(prefix, replacement_length));
    if (chars_removed != 0) {
      rejected_selection_start = position;
      rejected_selection_end = position + chars_removed;
    }

    const std::size_t fallback_caret_offset =
        chars_removed == 0 ? prefix : prefix + removed_length;
    const core::JwpPosition before_caret =
        document_->jwp_caret_.has_value() &&
                document_->jwp_document_->valid_position(*document_->jwp_caret_)
            ? *document_->jwp_caret_
            : core::jwp_plain_text_position(*document_->jwp_document_,
                                            fallback_caret_offset);
    core::JwpDocumentModel updated = *document_->jwp_document_;
    core::JwpDocumentHistory history = document_->jwp_history_;
    history.begin(*document_->jwp_document_, before_caret);
    const core::JwpPosition after_caret = core::replace_jwp_plain_text(
        updated, prefix, removed_length,
        std::u32string_view(current).substr(
            prefix, replacement_length),
        document_->jwp_code_page_);
    const core::JwpHistoryKind kind =
        removed_length == 0 && replacement_length != 0
            ? core::JwpHistoryKind::kTyping
        : removed_length != 0 && replacement_length == 0
            ? core::JwpHistoryKind::kDeletion
            : core::JwpHistoryKind::kNone;
    history.commit(updated, after_caret, kind);
    const bool modified = !document_->saved_jwp_document_.has_value() ||
                          updated.document() != *document_->saved_jwp_document_;
    document_->editor_->apply_kanji_colors(updated.document(), kanji_color_list_,
                                kanji_color_policy_, document_->jwp_code_page_);
    document_->jwp_document_ = std::move(updated);
    document_->jwp_history_ = std::move(history);
    document_->jwp_caret_ = after_caret;
    document_->expected_jwp_caret_ = after_caret;
    document_->rendered_jwp_text_ = std::move(current);
    document_->editor_->document()->setModified(modified);
    update_undo_actions();
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
  const bool modified = document_->saved_jwp_document_.has_value() &&
                        document_->jwp_document_->document() != *document_->saved_jwp_document_;
  document_->updating_editor_ = true;
  document_->editor_->undo();
  if (from_qstring(document_plain_text(*document_->editor_->document())) != document_->rendered_jwp_text_) {
    document_->editor_->setPlainText(to_qstring(document_->rendered_jwp_text_));
    apply_jwp_presentation(document_->jwp_document_->document(), document_->jwp_code_page_);
    QTextCursor cursor = document_->editor_->textCursor();
    cursor.setPosition(
        std::min(cursor_position, document_->editor_->document()->characterCount() - 1));
    document_->editor_->setTextCursor(cursor);
  } else {
    document_->editor_->apply_kanji_colors(document_->jwp_document_->document(), kanji_color_list_,
                                kanji_color_policy_, document_->jwp_code_page_);
  }
  if (selection_start >= 0 && selection_end >= selection_start) {
    QTextCursor cursor = document_->editor_->textCursor();
    cursor.setPosition(selection_start);
    cursor.setPosition(selection_end, QTextCursor::KeepAnchor);
    document_->editor_->setTextCursor(cursor);
  }
  document_->updating_editor_ = false;
  document_->editor_->document()->setModified(modified);
  update_title();
}

void MainWindow::update_encoding_display() {
  const bool jwp = document_->jwp_format_;
  encoding_label_->setText(
      jwp ? tr("JWP / %1").arg(code_page_name(document_->jwp_code_page_))
          : encoding_name(document_->encoding_));
  for (QAction* action : encoding_actions_->actions()) {
    action->setEnabled(!jwp);
    action->setChecked(!jwp && action->data().toInt() ==
                                   static_cast<int>(document_->encoding_));
  }
  if (jwp_code_page_menu_ != nullptr) {
    jwp_code_page_menu_->setEnabled(jwp || !document_->jwp_document_);
  }
  for (QAction* action : jwp_code_page_actions_) {
    action->setChecked(action->data().toInt() ==
                       static_cast<int>(document_->jwp_code_page_));
  }
}

core::TextEncoding MainWindow::text_encoding() const noexcept {
  return document_->encoding_;
}

bool MainWindow::is_jwp_document() const noexcept {
  return document_->jwp_document_.has_value();
}

bool MainWindow::uses_jwp_format() const noexcept { return document_->jwp_format_; }

bool MainWindow::set_japanese_editing(bool enabled, bool allow_information_loss,
                                     OpenMode mode) {
  if (enabled == is_jwp_document()) return true;
  if (conversion_active() || document_->kana_input_.pending()) return false;
  try {
    const auto report = document_->jwp_document_
        ? core::export_jwp_plain_text(*document_->jwp_document_, document_->jwp_code_page_)
        : core::JwpPlainTextExport{
              from_qstring(document_plain_text(*document_->editor_->document()))};
    const bool has_history = document_->jwp_document_
        ? document_->jwp_history_.can_undo() || document_->jwp_history_.can_redo()
        : document_->editor_->document()->isUndoAvailable() || document_->editor_->document()->isRedoAvailable();
    if (!allow_information_loss && (has_history || !report.lossless())) return false;

    std::optional<core::JwpDocumentModel> model;
    if (enabled) model = core::import_jwp_plain_text(report.text, document_->jwp_code_page_);
    auto saved_jwp = document_->saved_jwp_document_;
    if (enabled && !saved_jwp && document_->saved_text_file_) {
      try {
        saved_jwp = core::import_jwp_plain_text(document_->saved_text_file_->text, document_->jwp_code_page_)
                        .document();
      } catch (const core::JwpPlainTextError&) {
        // An unrepresentable saved version cannot be an unchanged native model.
      }
    }
    auto saved_text = document_->saved_text_file_;
    auto pristine = document_->pristine_jwp_document_;
    const bool modified = document_modified() || !report.lossless();
    const bool format = document_->jwp_format_;
    const auto encoding = document_->encoding_;
    const bool bom = document_->has_byte_order_mark_;
    const int anchor = document_->editor_->textCursor().anchor();
    const int position = document_->editor_->textCursor().position();
    if (model) {
      load_jwp_document(document_->current_path_, model->document(), document_->jwp_code_page_);
    } else {
      load_document(document_->current_path_, core::TextFile{report.text, encoding, bom}, false);
    }
    document_->jwp_format_ = format;
    document_->encoding_ = encoding;
    document_->has_byte_order_mark_ = bom;
    document_->saved_jwp_document_ = std::move(saved_jwp);
    document_->saved_text_file_ = std::move(saved_text);
    document_->pristine_jwp_document_ = std::move(pristine);
    {
      const QScopedValueRollback<bool> guard(document_->updating_editor_, true);
      QTextCursor restored(document_->editor_->document());
      restored.setPosition(anchor);
      restored.setPosition(position, QTextCursor::KeepAnchor);
      document_->editor_->setTextCursor(restored);
      document_->editor_->document()->setModified(modified);
    }
    update_encoding_display();
    update_title();
    return true;
  } catch (const std::exception& error) {
    if (mode == OpenMode::kInteractive) show_error(tr("Could not change editing mode"), error);
    return false;
  }
}

bool MainWindow::native_document_modified() const {
  return !document_->saved_jwp_document_ ||
      document_->jwp_document_->document() != *document_->saved_jwp_document_ ||
      (!document_->jwp_format_ && document_->saved_text_file_ &&
       (document_->encoding_ != document_->saved_text_file_->encoding ||
        document_->has_byte_order_mark_ != document_->saved_text_file_->has_byte_order_mark));
}

core::LegacyCodePage MainWindow::jwp_code_page() const noexcept {
  return document_->jwp_code_page_;
}

const core::JwpDocument* MainWindow::current_jwp_document() const noexcept {
  return document_->jwp_document_.has_value() ? &document_->jwp_document_->document() : nullptr;
}

QString MainWindow::current_path() const { return document_->current_path_; }

bool MainWindow::document_modified() const noexcept {
  return document_->editor_->document()->isModified() ||
      (!document_->jwp_format_ && document_->saved_text_file_ &&
       (document_->encoding_ != document_->saved_text_file_->encoding ||
        document_->has_byte_order_mark_ != document_->saved_text_file_->has_byte_order_mark));
}

bool MainWindow::prompt_to_revert(const QString& path) {
  return QMessageBox::question(
             this, tr("Revert document"),
             tr("Discard changes and reload %1?").arg(path),
             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) ==
         QMessageBox::Yes;
}

bool MainWindow::prompt_to_delete(const QString& path) {
  return QMessageBox::warning(
             this, tr("Delete file"),
             tr("Permanently delete %1?").arg(path),
             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) ==
         QMessageBox::Yes;
}

bool MainWindow::maybe_save() {
  if (conversion_active() && !accept_conversion()) {
    return false;
  }
  finish_kana_input();
  if (!document_modified()) {
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
  const QString name = document_->current_path_.isEmpty()
                           ? tr("Untitled")
                           : QFileInfo(document_->current_path_).fileName();
  setWindowTitle(tr("%1[*] - jwpqt").arg(name));
  setWindowModified(document_modified());
  const int index = current_document_index();
  if (index >= 0) {
    const QString tab_name = QString(name).replace(QLatin1Char('&'), QStringLiteral("&&"));
    document_tabs_->setTabText(
        index, tab_name + (document_modified() ? QStringLiteral(" *") : QString()));
    document_tabs_->setTabToolTip(index, document_->current_path_);
  }
  if (next_file_action_) next_file_action_->setEnabled(document_count() > 1);
  if (previous_file_action_) previous_file_action_->setEnabled(document_count() > 1);
  const bool has_path = !document_->current_path_.isEmpty();
  if (revert_action_ != nullptr)
    revert_action_->setEnabled(has_path && !conversion_active());
  if (delete_action_ != nullptr)
    delete_action_->setEnabled(has_path && !conversion_active());
}

void MainWindow::show_error(const QString& action,
                            const std::exception& error) {
  QMessageBox::critical(this, tr("jwpqt"),
                        action + QStringLiteral("\n\n") +
                            QString::fromUtf8(error.what()));
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (approve_close_all(OpenMode::kInteractive)) {
    if (application_settings_.save_settings_on_exit && application_settings_persistence_enabled_ &&
        !application_settings_path_.isEmpty()) {
      bool saved = false;
      try {
        const QFileInfo file(application_settings_path_);
        if (file.exists() || file.isSymLink())
          (void)read_application_settings_file(application_settings_path_);
        saved = save_application_settings();
      } catch (const std::exception& error) {
        application_settings_warning_ = tr("Could not save settings: %1").arg(QString::fromUtf8(error.what()));
        update_resource_status();
      }
      if (!saved && QMessageBox::warning(this, tr("Settings"),
          application_settings_warning_ + tr("\n\nExit without saving settings?"),
          QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Discard) {
        event->ignore();
        return;
      }
    }
    for (const auto& state : documents_) record_recent_document(*state);
    event->accept();
  } else {
    event->ignore();
  }
}

}  // namespace jwpqt::qt
