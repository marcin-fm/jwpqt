// SPDX-License-Identifier: GPL-2.0-or-later

#include "main_window.h"

#include <algorithm>
#include <array>
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScopedValueRollback>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

#include "edict_lookup_dialog.h"
#include "edict_resource_search.h"
#include "edict_results_window.h"
#include "edict_resources.h"
#include "edict_user_dictionary_dialog.h"
#include "file_io.h"
#include "jis_table_dialog.h"
#include "jwp_editor.h"
#include "kanji_code_lookup_dialog.h"
#include "kanji_count_dialog.h"
#include "kanji_info_dialog.h"
#include "kanji_lookup_dialog.h"
#include "kanji_reading_lookup_dialog.h"
#include "kanji_color_settings.h"
#include "jwpqt/core/jis_table.h"
#include "jwpqt/core/jwp_plain_text.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/plain_text_change.h"
#include "jwpqt/core/text_detection.h"
#include "text_bridge.h"
#include "wnn_user_dictionary_dialog.h"

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
      editor_(new JwpEditor(this)),
      encoding_label_(new QLabel(this)),
      undo_action_(nullptr),
      redo_action_(nullptr),
      input_mode_label_(new QLabel(this)),
      encoding_actions_(new QActionGroup(this)),
      jwp_code_page_menu_(nullptr) {
  setCentralWidget(editor_);
  editor_->installEventFilter(this);
  editor_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  editor_->setLineWrapMode(QTextEdit::WidgetWidth);

  create_actions();
  encoding_label_->setObjectName(QStringLiteral("documentEncoding"));
  input_mode_label_->setObjectName(QStringLiteral("inputMode"));
  statusBar()->addPermanentWidget(encoding_label_);
  statusBar()->addPermanentWidget(input_mode_label_);
  update_encoding_display();
  resize(900, 680);

  connect(editor_->document(), &QTextDocument::contentsChange, this,
          [this](int position, int chars_removed, int chars_added) {
            synchronize_jwp_document(position, chars_removed, chars_added);
          });
  connect(editor_->document(), &QTextDocument::modificationChanged, this,
          [this] { update_title(); });
  connect(editor_, &QTextEdit::cursorPositionChanged, this, [this] {
    if (updating_editor_ || conversion_active() ||
        !jwp_document_.has_value()) {
      return;
    }
    try {
      const std::size_t offset = utf32_offset_for_utf16(
          editor_->toPlainText(), editor_->textCursor().position());
      const core::JwpPosition caret =
          core::jwp_plain_text_position(*jwp_document_, offset);
      if (expected_jwp_caret_.has_value() &&
          caret == *expected_jwp_caret_) {
        expected_jwp_caret_.reset();
      } else {
        jwp_history_.break_coalescing();
        expected_jwp_caret_.reset();
      }
      jwp_caret_ = caret;
      if (automatic_conversion_range_.has_value() &&
          !applying_kana_input_ &&
          caret != automatic_conversion_range_->end) {
        clear_automatic_conversion_range();
      }
    } catch (const std::exception&) {
      jwp_caret_.reset();
      expected_jwp_caret_.reset();
      jwp_history_.break_coalescing();
    }
    update_kanji_info_action();
  });
  connect(editor_, &QTextEdit::selectionChanged, this,
          [this] { update_conversion_actions(); });
  update_undo_actions();
  update_conversion_actions();
  update_kana_input_state();
  update_title();
}

MainWindow::~MainWindow() {
  delete wnn_user_dictionary_dialog_;
  delete edict_lookup_dialog_;
  delete edict_results_window_;
  delete edict_user_dictionary_dialog_;
  delete kanji_info_dialog_;
  delete jis_table_dialog_;
  delete kanji_count_dialog_;
  delete kanji_code_lookup_dialog_;
  delete kanji_reading_lookup_dialog_;
  delete kanji_lookup_dialog_;
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
    delete kanji_info_dialog_;
    kanji_info_dialog_ = nullptr;
    kanji_info_database_ = std::move(candidate);
    kanji_info_path_ = path;
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

    if (jwp_document_.has_value()) {
      editor_->apply_kanji_colors(jwp_document_->document(), list, policy,
                                  jwp_code_page_);
    } else {
      editor_->clear_kanji_colors();
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

    if (jwp_document_.has_value()) {
      editor_->apply_kanji_colors(jwp_document_->document(),
                                  kanji_color_list_, policy, jwp_code_page_);
    }
    try {
      QSettings settings(kanji_color_settings_path_, QSettings::IniFormat);
      write_kanji_color_policy(settings, policy);
    } catch (...) {
      if (jwp_document_.has_value()) {
        editor_->apply_kanji_colors(jwp_document_->document(),
                                    kanji_color_list_, kanji_color_policy_,
                                    jwp_code_page_);
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
    if (jwp_document_.has_value()) {
      prepared_colors = editor_->prepare_kanji_colors(
          jwp_document_->document(), color_list, kanji_color_policy_,
          jwp_code_page_);
    }
    write_kanji_color_list_file(kanji_color_list_path_, color_list);
    kanji_color_list_ = std::move(color_list);
    if (jwp_document_.has_value()) {
      editor_->set_kanji_color_selections(std::move(prepared_colors));
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
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }
  core::KanjiColorList color_list;
  color_list.add_document(jwp_document_->document());
  return set_kanji_color_list(std::move(color_list), mode);
}

bool MainWindow::append_kanji_color_list(OpenMode mode) {
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }
  core::KanjiColorList color_list = kanji_color_list_;
  color_list.add_document(jwp_document_->document());
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
        core::encode_jwp_text(from_qstring(text), jwp_code_page_);
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
  if (conversion_active() || !maybe_save()) {
    return false;
  }
  try {
    core::JwpDocument document;
    document.paragraphs.emplace_back();
    document.paragraphs.front().text = kanji_color_list_.codes();
    load_jwp_document(QString(), std::move(document), jwp_code_page_);
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

  undo_action_ = edit_menu->addAction(tr("&Undo"));
  undo_action_->setObjectName(QStringLiteral("undoAction"));
  undo_action_->setShortcut(QKeySequence::Undo);
  undo_action_->setEnabled(false);
  connect(undo_action_, &QAction::triggered, this,
          [this] { undo_document(); });
  connect(editor_, &QTextEdit::undoAvailable, this,
          [this](bool available) {
            qt_undo_available_ = available;
            update_undo_actions();
          });

  redo_action_ = edit_menu->addAction(tr("&Redo"));
  redo_action_->setObjectName(QStringLiteral("redoAction"));
  redo_action_->setShortcut(QKeySequence::Redo);
  redo_action_->setEnabled(false);
  connect(redo_action_, &QAction::triggered, this,
          [this] { redo_document(); });
  connect(editor_, &QTextEdit::redoAvailable, this,
          [this](bool available) {
            qt_redo_available_ = available;
            update_undo_actions();
          });

  edit_menu->addSeparator();
  QAction* cut_action = edit_menu->addAction(tr("Cu&t"));
  cut_action->setShortcut(QKeySequence::Cut);
  cut_action->setEnabled(false);
  connect(cut_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    editor_->cut();
  });
  connect(editor_, &QTextEdit::copyAvailable, cut_action,
          &QAction::setEnabled);

  QAction* copy_action = edit_menu->addAction(tr("&Copy"));
  copy_action->setShortcut(QKeySequence::Copy);
  copy_action->setEnabled(false);
  connect(copy_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    editor_->copy();
  });
  connect(editor_, &QTextEdit::copyAvailable, copy_action,
          &QAction::setEnabled);

  QAction* paste_action = edit_menu->addAction(tr("&Paste"));
  paste_action->setShortcut(QKeySequence::Paste);
  connect(paste_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    editor_->paste();
  });

  edit_menu->addSeparator();
  QAction* select_all_action = edit_menu->addAction(tr("Select &All"));
  select_all_action->setShortcut(QKeySequence::SelectAll);
  connect(select_all_action, &QAction::triggered, this, [this] {
    finish_kana_input();
    editor_->selectAll();
  });

  // The standard QTextEdit menu would bypass portable JWP history.
  editor_->setContextMenuPolicy(Qt::ActionsContextMenu);
  editor_->addAction(undo_action_);
  editor_->addAction(redo_action_);
  editor_->addAction(cut_action);
  editor_->addAction(copy_action);
  editor_->addAction(paste_action);
  editor_->addAction(select_all_action);

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

  QMenu* format_menu = menuBar()->addMenu(tr("F&ormat"));
  format_paragraph_action_ =
      format_menu->addAction(tr("&Paragraph..."));
  format_paragraph_action_->setObjectName(
      QStringLiteral("formatParagraphAction"));
  format_paragraph_action_->setShortcut(
      QKeySequence(QStringLiteral("Alt+Shift+F")));
  connect(format_paragraph_action_, &QAction::triggered, this,
          [this] { format_document_paragraphs(); });

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

  kanji_info_action_ = tools_menu->addAction(tr("Kanji &Information"));
  kanji_info_action_->setObjectName(QStringLiteral("kanjiInfoAction"));
  kanji_info_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
  connect(kanji_info_action_, &QAction::triggered, this,
          [this] { show_kanji_info_dialog(); });

  jis_table_action_ = tools_menu->addAction(tr("&JIS Table"));
  jis_table_action_->setObjectName(QStringLiteral("jisTableAction"));
  jis_table_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
  connect(jis_table_action_, &QAction::triggered, this,
          [this] { show_jis_table_dialog(); });

  skip_lookup_action_ = tools_menu->addAction(tr("&SKIP Lookup"));
  skip_lookup_action_->setObjectName(QStringLiteral("skipLookupAction"));
  skip_lookup_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+S")));
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
  spahn_lookup_action_->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));
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
  kana_input_action_ = convert_menu->addAction(tr("&Kana Input"));
  kana_input_action_->setObjectName(QStringLiteral("kanaInputAction"));
  kana_input_action_->setCheckable(true);
  kana_input_action_->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+K")));
  connect(kana_input_action_, &QAction::toggled, this,
          [this](bool enabled) { set_kana_input_enabled(enabled); });

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
            [this, code_page] { set_jwp_code_page(code_page); });
  }
}

void MainWindow::undo_document() {
  finish_kana_input();
  if (!jwp_document_.has_value()) {
    editor_->undo();
    return;
  }
  try {
    core::JwpDocumentModel model = *jwp_document_;
    core::JwpDocumentHistory history = jwp_history_;
    core::JwpPosition caret = jwp_caret_.value_or(core::JwpPosition{});
    if (!history.undo(model, caret)) {
      return;
    }
    jwp_document_ = std::move(model);
    jwp_history_ = std::move(history);
    restore_jwp_history_state(caret);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not undo: %1").arg(QString::fromUtf8(error.what())), 5000);
  }
}

void MainWindow::redo_document() {
  finish_kana_input();
  if (!jwp_document_.has_value()) {
    editor_->redo();
    return;
  }
  try {
    core::JwpDocumentModel model = *jwp_document_;
    core::JwpDocumentHistory history = jwp_history_;
    core::JwpPosition caret = jwp_caret_.value_or(core::JwpPosition{});
    if (!history.redo(model, caret)) {
      return;
    }
    jwp_document_ = std::move(model);
    jwp_history_ = std::move(history);
    restore_jwp_history_state(caret);
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not redo: %1").arg(QString::fromUtf8(error.what())), 5000);
  }
}

void MainWindow::restore_jwp_history_state(core::JwpPosition caret) {
  std::u32string text =
      core::decode_jwp_plain_text(*jwp_document_, jwp_code_page_);
  const QString qt_text = to_qstring(text);
  const std::size_t offset =
      core::jwp_plain_text_offset(*jwp_document_, caret);
  const int qt_offset = utf16_offset_for_utf32(text, offset);

  updating_editor_ = true;
  editor_->setPlainText(qt_text);
  apply_jwp_presentation(jwp_document_->document(), jwp_code_page_);
  QTextCursor cursor = editor_->textCursor();
  cursor.setPosition(qt_offset);
  editor_->setTextCursor(cursor);
  updating_editor_ = false;

  rendered_jwp_text_ = std::move(text);
  jwp_caret_ = caret;
  expected_jwp_caret_.reset();
  const bool modified = !saved_jwp_document_.has_value() ||
                        jwp_document_->document() != *saved_jwp_document_;
  editor_->document()->setModified(modified);
  update_undo_actions();
  update_title();
}

void MainWindow::apply_jwp_presentation(const core::JwpDocument& document,
                                        core::LegacyCodePage code_page) {
  editor_->apply_jwp_layout(document);
  editor_->apply_kanji_colors(document, kanji_color_list_,
                              kanji_color_policy_, code_page);
}

void MainWindow::clear_jwp_presentation() {
  editor_->clear_jwp_layout();
  editor_->clear_kanji_colors();
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
  const bool jwp = jwp_document_.has_value();
  undo_action_->setEnabled(jwp ? jwp_history_.can_undo()
                               : qt_undo_available_);
  redo_action_->setEnabled(jwp ? jwp_history_.can_redo()
                               : qt_redo_available_);
}

void MainWindow::update_conversion_actions() {
  if (convert_action_ == nullptr) {
    return;
  }
  const bool active = conversion_active();
  bool can_convert = false;
  if (!active && jwp_document_.has_value() && wnn_resources_ != nullptr) {
    const QTextCursor cursor = editor_->textCursor();
    if (cursor.hasSelection()) {
      try {
        const QString text = editor_->toPlainText();
        const core::JwpPosition begin = core::jwp_plain_text_position(
            *jwp_document_,
            utf32_offset_for_utf16(text, cursor.selectionStart()));
        const core::JwpPosition end = core::jwp_plain_text_position(
            *jwp_document_,
            utf32_offset_for_utf16(text, cursor.selectionEnd()));
        can_convert = begin.paragraph == end.paragraph && begin != end;
      } catch (const std::exception&) {
      }
    }
  }
  convert_action_->setEnabled(can_convert);
  previous_candidate_action_->setEnabled(active);
  next_candidate_action_->setEnabled(active);
  accept_candidate_action_->setEnabled(active);
  user_dictionary_action_->setEnabled(!active && wnn_resources_ != nullptr);
  format_paragraph_action_->setEnabled(!active && jwp_document_.has_value());
  insert_page_break_action_->setEnabled(!active && jwp_document_.has_value());
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
    kanji_info_action_->setEnabled(kanji_info_target().has_value());
  }
}

void MainWindow::update_jis_table_action() {
  if (jis_table_action_ != nullptr) {
    jis_table_action_->setEnabled(jwp_document_.has_value() &&
                                  !conversion_active());
  }
}

void MainWindow::update_kanji_count_action() {
  if (kanji_count_action_ != nullptr) {
    kanji_count_action_->setEnabled(jwp_document_.has_value() &&
                                    !conversion_active());
  }
}

void MainWindow::update_kanji_code_lookup_actions() {
  const bool enabled = kanji_info_database_ != nullptr &&
                       jwp_document_.has_value() && !conversion_active();
  if (skip_lookup_action_ != nullptr) skip_lookup_action_->setEnabled(enabled);
  if (four_corner_lookup_action_ != nullptr)
    four_corner_lookup_action_->setEnabled(enabled);
  if (bushu_lookup_action_ != nullptr)
    bushu_lookup_action_->setEnabled(enabled);
  if (stroke_bushu_lookup_action_ != nullptr)
    stroke_bushu_lookup_action_->setEnabled(enabled);
  if (spahn_lookup_action_ != nullptr)
    spahn_lookup_action_->setEnabled(enabled);
}

void MainWindow::update_kanji_reading_lookup_action() {
  if (kanji_reading_lookup_action_ != nullptr) {
    kanji_reading_lookup_action_->setEnabled(
        kanji_info_database_ != nullptr && jwp_document_.has_value() &&
        !conversion_active());
  }
}

void MainWindow::update_kanji_lookup_action() {
  if (kanji_lookup_action_ != nullptr) {
    kanji_lookup_action_->setEnabled(has_kanji_lookup() &&
                                     jwp_document_.has_value() &&
                                     !conversion_active());
  }
}

void MainWindow::update_kanji_color_actions() {
  if (kanji_color_options_action_ == nullptr) {
    return;
  }
  const bool active = conversion_active();
  const bool configured = !kanji_color_list_path_.isEmpty();
  const bool jwp = jwp_document_.has_value();
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
  return kana_input_enabled_;
}

void MainWindow::set_kana_input_enabled(bool enabled) {
  if (enabled && !jwp_document_.has_value()) {
    enabled = false;
  }
  if (kana_input_enabled_ != enabled) {
    if (!enabled) {
      finish_kana_input();
    }
    reset_kana_input(false);
    kana_input_enabled_ = enabled;
  }
  if (kana_input_action_ != nullptr &&
      kana_input_action_->isChecked() != kana_input_enabled_) {
    const QSignalBlocker blocker(kana_input_action_);
    kana_input_action_->setChecked(kana_input_enabled_);
  }
  update_kana_input_state();
}

void MainWindow::update_kana_input_state() {
  if (kana_input_action_ == nullptr || input_mode_label_ == nullptr) {
    return;
  }
  kana_input_action_->setEnabled(jwp_document_.has_value() &&
                                 !conversion_active());
  input_mode_label_->setText(kana_input_enabled_ ? tr("Kana") : tr("Direct"));
}

void MainWindow::apply_kana_input_events(
    const std::vector<core::KanaInputEvent>& events) {
  if (events.empty() || !jwp_document_.has_value()) {
    return;
  }

  const QScopedValueRollback<bool> applying(applying_kana_input_, true);
  bool force_after_events = false;
  for (const core::KanaInputEvent& event : events) {
    if (event.text.empty()) {
      continue;
    }

    if (event.kind == core::KanaInputKind::kKanaStart &&
        automatic_conversion_range_.has_value()) {
      if (attempt_automatic_conversion(true)) {
        accept_conversion();
      }
      clear_automatic_conversion_range();
    }

    QTextCursor cursor = editor_->textCursor();
    const QString before = editor_->toPlainText();
    core::JwpPosition insertion_begin = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(before, cursor.selectionStart()));
    bool extends_automatic =
        automatic_conversion_range_.has_value() && !cursor.hasSelection() &&
        insertion_begin == automatic_conversion_range_->end;
    if (automatic_conversion_range_.has_value() && !extends_automatic) {
      if (attempt_automatic_conversion(true)) {
        accept_conversion();
      }
      clear_automatic_conversion_range();
      cursor = editor_->textCursor();
      const QString current = editor_->toPlainText();
      insertion_begin = core::jwp_plain_text_position(
          *jwp_document_,
          utf32_offset_for_utf16(current, cursor.selectionStart()));
      extends_automatic = false;
    }

    editor_->insertPlainText(
        to_qstring(core::decode_jwp_text(event.text, jwp_code_page_)));
    if (!jwp_caret_.has_value() ||
        jwp_caret_->paragraph != insertion_begin.paragraph ||
        jwp_caret_->offset < insertion_begin.offset) {
      throw core::JwpConversionError(
          "kana input did not produce a valid document range");
    }

    if (event.kind == core::KanaInputKind::kKanaStart) {
      automatic_conversion_range_ =
          core::JwpRange{insertion_begin, *jwp_caret_};
      force_after_events = false;
    } else if (event.kind == core::KanaInputKind::kKanaContinue &&
               extends_automatic) {
      automatic_conversion_range_->end = *jwp_caret_;
    } else if (event.kind == core::KanaInputKind::kText &&
               extends_automatic) {
      force_after_events = true;
    }
  }

  if (automatic_conversion_range_.has_value()) {
    attempt_automatic_conversion(force_after_events);
  }
}

void MainWindow::finish_kana_input() {
  if (kana_input_.pending()) {
    try {
      apply_kana_input_events(kana_input_.flush());
    } catch (const core::KanaInputError&) {
      kana_input_.discard();
    }
  }
  if (automatic_conversion_range_.has_value()) {
    attempt_automatic_conversion(true);
  }
  if (conversion_active()) {
    accept_conversion();
  }
}

void MainWindow::reset_kana_input(bool disable_mode) {
  kana_input_.discard();
  clear_automatic_conversion_range();
  if (disable_mode) {
    kana_input_enabled_ = false;
    if (kana_input_action_ != nullptr) {
      const QSignalBlocker blocker(kana_input_action_);
      kana_input_action_->setChecked(false);
    }
  }
  update_kana_input_state();
}

bool MainWindow::attempt_automatic_conversion(bool force) {
  if (!automatic_conversion_range_.has_value()) {
    return false;
  }
  if (!jwp_document_.has_value() || wnn_resources_ == nullptr) {
    clear_automatic_conversion_range();
    return false;
  }

  try {
    const core::JwpRange pending_range = *automatic_conversion_range_;
    if (pending_range.begin.paragraph != pending_range.end.paragraph ||
        pending_range.begin.paragraph >=
            jwp_document_->document().paragraphs.size()) {
      throw core::JwpConversionError(
          "automatic conversion range is invalid");
    }
    const core::JwpText& paragraph =
        jwp_document_->document()
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
    core::JwpPosition caret = jwp_caret_.value_or(pending_range.end);
    if (caret.paragraph != conversion_range.begin.paragraph ||
        caret.offset < conversion_range.end.offset) {
      caret = conversion_range.end;
    }

    auto transaction = std::make_unique<core::JwpConversionTransaction>(
        *jwp_document_, jwp_history_, wnn_resources_->session);
    conversion_preferences_before_ = wnn_resources_->preferences;
    transaction->begin_prepared(conversion_range, caret,
                                std::move(*prepared));
    clear_automatic_conversion_range();
    jwp_conversion_ = std::move(transaction);
    editor_->setReadOnly(true);
    restore_jwp_conversion_state();
    return true;
  } catch (const std::exception& error) {
    if (conversion_active()) {
      rollback_conversion_noexcept();
    } else {
      conversion_preferences_before_.reset();
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
  automatic_conversion_range_.reset();
  if (!conversion_active()) {
    editor_->set_transient_extra_selections({});
  }
}

void MainWindow::show_automatic_conversion_range() {
  if (!automatic_conversion_range_.has_value() ||
      !jwp_document_.has_value()) {
    editor_->set_transient_extra_selections({});
    return;
  }
  const std::u32string text =
      core::decode_jwp_plain_text(*jwp_document_, jwp_code_page_);
  const core::JwpRange range = *automatic_conversion_range_;
  QTextCursor cursor(editor_->document());
  cursor.setPosition(utf16_offset_for_utf32(
      text, core::jwp_plain_text_offset(*jwp_document_, range.begin)));
  cursor.setPosition(utf16_offset_for_utf32(
                         text, core::jwp_plain_text_offset(*jwp_document_,
                                                          range.end)),
                     QTextCursor::KeepAnchor);
  QTextEdit::ExtraSelection selection;
  selection.cursor = cursor;
  QColor highlight = editor_->palette().color(QPalette::Highlight);
  highlight.setAlpha(80);
  selection.format.setBackground(highlight);
  editor_->set_transient_extra_selections({selection});
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched != editor_ || !kana_input_enabled_ ||
      !jwp_document_.has_value() || conversion_active()) {
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
  if (kana_input_.pending() &&
      (key_event->key() == Qt::Key_Backspace ||
       key_event->key() == Qt::Key_Delete ||
       key_event->key() == Qt::Key_Escape)) {
    kana_input_.discard();
    statusBar()->showMessage(tr("Discarded pending kana input"), 1500);
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
            kana_input_.push_ascii(static_cast<char>(value)));
      } catch (const std::exception& error) {
        kana_input_.discard();
        statusBar()->showMessage(
            tr("Could not compose kana: %1")
                .arg(QString::fromUtf8(error.what())),
            5000);
      }
      return true;
    }
  }

  if (kana_input_.pending() && key_event->key() != Qt::Key_Shift &&
      key_event->key() != Qt::Key_Control && key_event->key() != Qt::Key_Alt &&
      key_event->key() != Qt::Key_Meta &&
      key_event->key() != Qt::Key_CapsLock) {
    finish_kana_input();
  } else if (automatic_conversion_range_.has_value() &&
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

void MainWindow::show_kanji_info_dialog() {
  const std::optional<core::JisCode> target = kanji_info_target();
  if (!target.has_value()) {
    statusBar()->showMessage(tr("No kanji information is available here"),
                             3000);
    return;
  }
  show_kanji_info_code(*target);
}

void MainWindow::show_kanji_info_code(core::JisCode code) {
  if (kanji_info_database_ == nullptr ||
      !kanji_info_database_->contains(code)) {
    statusBar()->showMessage(tr("No information is available for this kanji"),
                             3000);
    return;
  }
  if (kanji_info_dialog_ != nullptr) {
    if (!kanji_info_dialog_->set_code(code)) return;
    kanji_info_dialog_->show();
    kanji_info_dialog_->raise();
    kanji_info_dialog_->activateWindow();
    return;
  }
  auto* dialog = new KanjiInfoDialog(*kanji_info_database_, this);
  if (!dialog->set_code(code)) {
    delete dialog;
    return;
  }
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { kanji_info_dialog_ = nullptr; });
  kanji_info_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_jis_table_dialog() {
  if (!jwp_document_.has_value() || conversion_active()) {
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
  if (!jwp_document_.has_value() || conversion_active()) {
    statusBar()->showMessage(tr("Count Kanji is not available"), 3000);
    return;
  }
  finish_kana_input();
  if (!jwp_document_.has_value() || conversion_active())
    return;
  if (kanji_count_dialog_ != nullptr) {
    kanji_count_dialog_->show();
    kanji_count_dialog_->raise();
    kanji_count_dialog_->activateWindow();
    return;
  }
  try {
    auto* dialog = new KanjiCountDialog(
        {&jwp_document_->document()}, kanji_color_list_,
        kanji_info_database_ != nullptr ? kanji_info_database_.get() : nullptr,
        [this](std::u32string text) { insert_edict_text(std::move(text)); },
        [this](core::JisCode code) { show_kanji_info_code(code); }, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QObject::destroyed, this,
            [this] { kanji_count_dialog_ = nullptr; });
    kanji_count_dialog_ = dialog;
    dialog->show();
  } catch (const std::exception& error) {
    show_error(tr("Could not open Count Kanji"), error);
  }
}

void MainWindow::show_kanji_code_lookup_dialog(KanjiCodeLookupMode mode) {
  if (kanji_info_database_ == nullptr || !jwp_document_.has_value() ||
      conversion_active()) {
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
  if (kanji_info_database_ == nullptr || !jwp_document_.has_value() ||
      conversion_active()) {
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
  if (!seed.empty()) dialog->set_query_text(seed);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { kanji_reading_lookup_dialog_ = nullptr; });
  kanji_reading_lookup_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_kanji_lookup_dialog() {
  if (!has_kanji_lookup() || !jwp_document_.has_value() ||
      conversion_active()) {
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
  if (!jwp_document_.has_value()) {
    return std::nullopt;
  }
  try {
    const QTextCursor cursor = editor_->textCursor();
    const int qt_offset = cursor.hasSelection() ? cursor.selectionStart()
                                                : cursor.position();
    const core::JwpPosition position = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(editor_->toPlainText(), qt_offset));
    if (position.paragraph >= jwp_document_->paragraph_count()) {
      return std::nullopt;
    }
    const core::JwpText& text =
        jwp_document_->document().paragraphs[position.paragraph].text;
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

std::optional<core::JisCode> MainWindow::kanji_info_target() const {
  if (kanji_info_database_ == nullptr) return std::nullopt;
  const std::optional<core::JisCode> code = jwp_character_target();
  return code.has_value() && kanji_info_database_->contains(*code)
             ? code
             : std::nullopt;
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
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }

  try {
    const core::JwpText inserted = core::render_wnn_user_entry(entry);
    const QString original_text = editor_->toPlainText();
    const QTextCursor original_cursor = editor_->textCursor();
    const bool original_modified = editor_->document()->isModified();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(original_text, original_cursor.position()));
    const core::JwpPosition selection_begin = core::jwp_plain_text_position(
        *jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionStart()));
    const core::JwpPosition selection_end = core::jwp_plain_text_position(
        *jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionEnd()));

    core::JwpDocumentModel candidate = *jwp_document_;
    core::JwpDocumentHistory history = jwp_history_;
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
        core::decode_jwp_plain_text(candidate, jwp_code_page_);
    const QString qt_text = to_qstring(rendered);
    const int qt_caret = utf16_offset_for_utf32(
        rendered, core::jwp_plain_text_offset(candidate, following));

    updating_editor_ = true;
    try {
      editor_->setPlainText(qt_text);
      apply_jwp_presentation(candidate.document(), jwp_code_page_);
      QTextCursor cursor(editor_->document());
      cursor.setPosition(qt_caret);
      editor_->setTextCursor(cursor);
    } catch (...) {
      editor_->setPlainText(original_text);
      apply_jwp_presentation(jwp_document_->document(), jwp_code_page_);
      editor_->setTextCursor(original_cursor);
      editor_->document()->setModified(original_modified);
      updating_editor_ = false;
      throw;
    }
    updating_editor_ = false;

    jwp_document_ = std::move(candidate);
    jwp_history_ = std::move(history);
    jwp_caret_ = following;
    expected_jwp_caret_.reset();
    rendered_jwp_text_ = std::move(rendered);
    editor_->document()->setModified(
        !saved_jwp_document_.has_value() ||
        jwp_document_->document() != *saved_jwp_document_);
    update_undo_actions();
    update_conversion_actions();
    update_title();
    statusBar()->showMessage(tr("Inserted user conversion"), 2000);
    return true;
  } catch (const std::exception& error) {
    updating_editor_ = false;
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
        show_edict_results_window();
        if (edict_results_window_ != nullptr) {
          edict_results_window_->append_report(report);
        }
        return report;
      },
      [this](const std::u32string& text) { return insert_edict_text(text); },
      this);
  if (!seed.empty()) {
    dialog->set_query(seed);
  }
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  connect(dialog, &QObject::destroyed, this,
          [this] { edict_lookup_dialog_ = nullptr; });
  edict_lookup_dialog_ = dialog;
  dialog->show();
}

void MainWindow::show_edict_results_window() {
  if (edict_results_window_ != nullptr) {
    edict_results_window_->show();
    edict_results_window_->raise();
    edict_results_window_->activateWindow();
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
  results->show();
}

std::u32string MainWindow::edict_query_seed() const {
  QTextCursor cursor = editor_->textCursor();
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::WordUnderCursor);
    if (!cursor.hasSelection()) {
      const int position = cursor.position();
      const int length = editor_->document()->characterCount() - 1;
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
  if (text.empty() || conversion_active() || !jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }

  try {
    const QString original_text = editor_->toPlainText();
    const QTextCursor original_cursor = editor_->textCursor();
    const bool original_modified = editor_->document()->isModified();
    const std::size_t caret_offset = utf32_offset_for_utf16(
        original_text, original_cursor.position());
    const std::size_t selection_begin = utf32_offset_for_utf16(
        original_text, original_cursor.selectionStart());
    const std::size_t selection_end = utf32_offset_for_utf16(
        original_text, original_cursor.selectionEnd());
    const core::JwpPosition caret =
        core::jwp_plain_text_position(*jwp_document_, caret_offset);

    core::JwpDocumentModel candidate = *jwp_document_;
    core::JwpDocumentHistory history = jwp_history_;
    history.begin(candidate, caret);
    const core::JwpPosition following = core::replace_jwp_plain_text(
        candidate, selection_begin, selection_end - selection_begin, text,
        jwp_code_page_);
    if (!history.commit(candidate, following)) {
      return false;
    }

    std::u32string rendered =
        core::decode_jwp_plain_text(candidate, jwp_code_page_);
    const int qt_caret = utf16_offset_for_utf32(
        rendered, core::jwp_plain_text_offset(candidate, following));
    QScopedValueRollback<bool> update_guard(updating_editor_, true);
    try {
      editor_->setPlainText(to_qstring(rendered));
      apply_jwp_presentation(candidate.document(), jwp_code_page_);
      QTextCursor cursor(editor_->document());
      cursor.setPosition(qt_caret);
      editor_->setTextCursor(cursor);
    } catch (...) {
      editor_->setPlainText(original_text);
      apply_jwp_presentation(jwp_document_->document(), jwp_code_page_);
      editor_->setTextCursor(original_cursor);
      editor_->document()->setModified(original_modified);
      throw;
    }

    jwp_document_ = std::move(candidate);
    jwp_history_ = std::move(history);
    jwp_caret_ = following;
    expected_jwp_caret_.reset();
    rendered_jwp_text_ = std::move(rendered);
    editor_->document()->setModified(
        !saved_jwp_document_.has_value() ||
        jwp_document_->document() != *saved_jwp_document_);
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
  return jwp_conversion_ != nullptr && jwp_conversion_->active();
}

bool MainWindow::convert_selection() {
  if (conversion_active()) {
    return cycle_conversion();
  }
  if (!jwp_document_.has_value() || wnn_resources_ == nullptr) {
    statusBar()->showMessage(tr("WNN conversion is not available"), 3000);
    return false;
  }
  finish_kana_input();

  try {
    const QTextCursor cursor = editor_->textCursor();
    if (!cursor.hasSelection()) {
      statusBar()->showMessage(tr("Select kana to convert"), 3000);
      return false;
    }
    const QString text = editor_->toPlainText();
    const core::JwpPosition begin = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(text, cursor.selectionStart()));
    const core::JwpPosition end = core::jwp_plain_text_position(
        *jwp_document_, utf32_offset_for_utf16(text, cursor.selectionEnd()));
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *jwp_document_, utf32_offset_for_utf16(text, cursor.position()));

    auto transaction = std::make_unique<core::JwpConversionTransaction>(
        *jwp_document_, jwp_history_, wnn_resources_->session);
    conversion_preferences_before_ = wnn_resources_->preferences;
    if (!transaction->begin({begin, end}, caret)) {
      conversion_preferences_before_.reset();
      statusBar()->showMessage(tr("No conversion candidates"), 3000);
      return false;
    }
    jwp_conversion_ = std::move(transaction);
    editor_->setReadOnly(true);
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
    const bool changed = previous ? jwp_conversion_->cycle_previous()
                                  : jwp_conversion_->cycle_next();
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
    const core::JwpPosition caret = jwp_conversion_->caret();
    jwp_conversion_->accept();
    jwp_conversion_.reset();
    conversion_preferences_before_.reset();
    editor_->set_transient_extra_selections({});
    editor_->setReadOnly(false);
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
  if (!conversion_active() || !jwp_document_.has_value()) {
    throw core::JwpConversionError("native conversion is not active");
  }
  std::u32string text =
      core::decode_jwp_plain_text(*jwp_document_, jwp_code_page_);
  const core::JwpRange range = jwp_conversion_->range();
  const core::JwpPosition caret = jwp_conversion_->caret();
  const int begin = utf16_offset_for_utf32(
      text, core::jwp_plain_text_offset(*jwp_document_, range.begin));
  const int end = utf16_offset_for_utf32(
      text, core::jwp_plain_text_offset(*jwp_document_, range.end));

  updating_editor_ = true;
  editor_->setPlainText(to_qstring(text));
  apply_jwp_presentation(jwp_document_->document(), jwp_code_page_);
  QTextCursor cursor = editor_->textCursor();
  editor_->set_transient_extra_selections({});
  if (caret == range.begin) {
    cursor.setPosition(end);
    cursor.setPosition(begin, QTextCursor::KeepAnchor);
  } else if (caret == range.end) {
    cursor.setPosition(begin);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
  } else {
    const int caret_offset = utf16_offset_for_utf32(
        text, core::jwp_plain_text_offset(*jwp_document_, caret));
    cursor.setPosition(caret_offset);
    QTextEdit::ExtraSelection selection;
    QTextCursor selected(editor_->document());
    selected.setPosition(begin);
    selected.setPosition(end, QTextCursor::KeepAnchor);
    selection.cursor = selected;
    selection.format.setBackground(
        editor_->palette().brush(QPalette::Highlight));
    selection.format.setForeground(
        editor_->palette().brush(QPalette::HighlightedText));
    editor_->set_transient_extra_selections({selection});
  }
  editor_->setTextCursor(cursor);
  updating_editor_ = false;

  rendered_jwp_text_ = std::move(text);
  jwp_caret_ = caret;
  expected_jwp_caret_.reset();
  editor_->document()->setModified(
      !saved_jwp_document_.has_value() ||
      jwp_document_->document() != *saved_jwp_document_);
  const std::size_t selected = jwp_conversion_->selected_index() + 1U;
  const std::size_t total = jwp_conversion_->result().candidates.size();
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
      caret = jwp_conversion_->rollback();
    } catch (...) {
    }
  }
  jwp_conversion_.reset();
  if (conversion_preferences_before_.has_value()) {
    wnn_resources_->preferences =
        std::move(*conversion_preferences_before_);
    conversion_preferences_before_.reset();
  }
  editor_->setReadOnly(false);
  editor_->set_transient_extra_selections({});
  if (caret.has_value() && jwp_document_.has_value()) {
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
  reset_kana_input(true);
  jwp_document_.reset();
  saved_jwp_document_.reset();
  pristine_jwp_document_.reset();
  jwp_history_.clear();
  jwp_caret_.reset();
  expected_jwp_caret_.reset();
  rendered_jwp_text_.clear();
  updating_editor_ = true;
  editor_->clear();
  clear_jwp_presentation();
  updating_editor_ = false;
  editor_->document()->setModified(false);
  current_path_.clear();
  has_byte_order_mark_ = false;
  set_text_encoding(core::TextEncoding::kUtf8, false);
  update_encoding_display();
  update_undo_actions();
  update_conversion_actions();
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
  if (conversion_active() && !accept_conversion()) {
    return false;
  }
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
  if (conversion_active() && !accept_conversion()) {
    return false;
  }
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
  if (conversion_active() && !accept_conversion()) {
    return false;
  }
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
  reset_kana_input(true);
  jwp_document_.reset();
  saved_jwp_document_.reset();
  pristine_jwp_document_.reset();
  jwp_history_.clear();
  jwp_caret_.reset();
  expected_jwp_caret_.reset();
  rendered_jwp_text_.clear();
  updating_editor_ = true;
  editor_->setPlainText(to_qstring(file.text));
  clear_jwp_presentation();
  updating_editor_ = false;
  editor_->document()->setModified(false);
  current_path_ = path;
  encoding_ = file.encoding;
  has_byte_order_mark_ = file.has_byte_order_mark;
  update_encoding_display();
  update_undo_actions();
  update_conversion_actions();
  update_title();
}

void MainWindow::load_jwp_document(const QString& path,
                                    core::JwpDocument document,
                                    core::LegacyCodePage code_page) {
  reset_kana_input(false);
  std::optional<core::JwpDocument> pristine_document;
  if (document.paragraphs.empty()) {
    pristine_document = document;
  }
  core::JwpDocumentModel model(std::move(document));
  std::u32string text = core::decode_jwp_plain_text(model, code_page);

  // Validate every presentation transformation before replacing the live
  // document. The live calls below then cannot fail on text/model mismatch.
  JwpEditor staged_editor;
  staged_editor.setFont(editor_->font());
  staged_editor.setPlainText(to_qstring(text));
  staged_editor.apply_jwp_layout(model.document());
  staged_editor.prepare_kanji_colors(model.document(), kanji_color_list_,
                                     kanji_color_policy_, code_page);

  {
    QScopedValueRollback<bool> update_guard(updating_editor_, true);
    editor_->setPlainText(to_qstring(text));
    apply_jwp_presentation(model.document(), code_page);
  }
  jwp_document_ = std::move(model);
  saved_jwp_document_ = jwp_document_->document();
  pristine_jwp_document_ = std::move(pristine_document);
  jwp_history_.clear();
  jwp_caret_ = core::JwpPosition{};
  expected_jwp_caret_.reset();
  rendered_jwp_text_ = std::move(text);
  jwp_code_page_ = code_page;
  current_path_ = path;
  has_byte_order_mark_ = false;
  editor_->document()->setModified(false);
  update_encoding_display();
  update_undo_actions();
  update_conversion_actions();
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
  if (conversion_active() && !accept_conversion()) {
    return false;
  }
  finish_kana_input();
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
  finish_kana_input();
  if (conversion_active() && !accept_conversion()) {
    return;
  }
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
    apply_jwp_presentation(jwp_document_->document(), code_page);
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

void MainWindow::format_document_paragraphs() {
  finish_kana_input();
  if (conversion_active() || !jwp_document_.has_value()) {
    return;
  }
  try {
    const QString text = editor_->toPlainText();
    const QTextCursor cursor = editor_->textCursor();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(text, cursor.position()));
    const std::optional<core::JwpParagraphFormat> format =
        prompt_for_paragraph_format(
            jwp_document_->paragraph_format(caret.paragraph));
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
          core::encode_jwp_text(from_qstring(text), jwp_code_page_);
      const std::u32string replacement_text = from_qstring(replacement);
      const std::vector<core::JwpRange> ranges =
          core::find_all(*jwp_document_, pattern, options);
      candidate_jwp.emplace(jwp_document_->document());
      candidate_history.emplace(jwp_history_);
      std::size_t previous_original_end = 0;
      std::size_t current_offset = 0;
      for (const core::JwpRange& range : ranges) {
        const std::size_t begin =
            core::jwp_plain_text_offset(*jwp_document_, range.begin);
        const std::size_t end =
            core::jwp_plain_text_offset(*jwp_document_, range.end);
        current_offset += begin - previous_original_end;
        const core::JwpPosition before = core::jwp_plain_text_position(
            *candidate_jwp, current_offset);
        candidate_history->begin(*candidate_jwp, before);
        candidate_caret = core::replace_jwp_plain_text(
            *candidate_jwp, current_offset, end - begin, replacement_text,
            jwp_code_page_);
        candidate_history->commit(*candidate_jwp, *candidate_caret);
        current_offset =
            core::jwp_plain_text_offset(*candidate_jwp, *candidate_caret);
        previous_original_end = end;
        matches.emplace_back(
            utf16_offset_for_utf32(rendered_jwp_text_, begin),
            utf16_offset_for_utf32(rendered_jwp_text_, end));
      }
      std::reverse(matches.begin(), matches.end());
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
        apply_jwp_presentation(jwp_document_->document(), jwp_code_page_);
        editor_->setTextCursor(original_cursor);
        editor_->document()->setModified(original_modified);
        throw core::JwpPlainTextError(
            "Native editor did not apply the validated JWP replacements");
      }
      if (candidate_jwp.has_value() && candidate_caret.has_value()) {
        const std::size_t caret_offset = core::jwp_plain_text_offset(
            *candidate_jwp, *candidate_caret);
        QTextCursor caret(editor_->document());
        caret.setPosition(
            utf16_offset_for_utf32(expected_jwp_text, caret_offset));
        editor_->setTextCursor(caret);
      }
    }
    if (candidate_jwp.has_value()) {
      const bool modified = saved_jwp_document_.has_value() &&
                            candidate_jwp->document() != *saved_jwp_document_;
      editor_->apply_kanji_colors(candidate_jwp->document(),
                                  kanji_color_list_, kanji_color_policy_,
                                  jwp_code_page_);
      jwp_document_.emplace(std::move(*candidate_jwp));
      jwp_history_ = std::move(*candidate_history);
      jwp_caret_ = candidate_caret;
      expected_jwp_caret_.reset();
      rendered_jwp_text_ = std::move(expected_jwp_text);
      editor_->document()->setModified(modified);
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
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }

  try {
    const QString text = editor_->toPlainText();
    const QTextCursor cursor = editor_->textCursor();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(text, cursor.position()));
    const core::JwpPosition selection_begin = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(text, cursor.selectionStart()));
    const core::JwpPosition selection_end = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(text, cursor.selectionEnd()));

    core::JwpDocumentModel candidate = *jwp_document_;
    core::JwpDocumentHistory history = jwp_history_;
    history.begin(candidate, caret);
    candidate.format_paragraphs(selection_begin.paragraph,
                                selection_end.paragraph, format);
    if (!history.commit(candidate, caret)) {
      return true;
    }
    const int page_width = editor_->character_page_width();
    if (page_width > 0 &&
        (format.left_indent + format.right_indent >= page_width ||
         format.left_indent + format.right_indent + format.first_indent >=
             page_width)) {
      throw core::JwpDocumentEditError(
          "paragraph indents leave no usable line width");
    }

    apply_jwp_presentation(candidate.document(), jwp_code_page_);
    jwp_document_ = std::move(candidate);
    jwp_history_ = std::move(history);
    jwp_caret_ = caret;
    expected_jwp_caret_.reset();
    editor_->setTextCursor(cursor);
    const bool modified = !saved_jwp_document_.has_value() ||
                          jwp_document_->document() != *saved_jwp_document_;
    editor_->document()->setModified(modified);
    update_undo_actions();
    update_title();
    statusBar()->showMessage(tr("Paragraph format applied"), 2000);
    return true;
  } catch (const std::exception& error) {
    statusBar()->showMessage(
        tr("Could not format paragraphs: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

bool MainWindow::insert_page_break() {
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }
  finish_kana_input();
  if (conversion_active() || !jwp_document_.has_value()) {
    return false;
  }

  try {
    const QString original_text = editor_->toPlainText();
    const QTextCursor original_cursor = editor_->textCursor();
    const bool original_modified = editor_->document()->isModified();
    const core::JwpPosition caret = core::jwp_plain_text_position(
        *jwp_document_,
        utf32_offset_for_utf16(original_text, original_cursor.position()));
    const core::JwpPosition selection_begin = core::jwp_plain_text_position(
        *jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionStart()));
    const core::JwpPosition selection_end = core::jwp_plain_text_position(
        *jwp_document_, utf32_offset_for_utf16(
                            original_text, original_cursor.selectionEnd()));

    core::JwpDocumentModel candidate = *jwp_document_;
    core::JwpDocumentHistory history = jwp_history_;
    history.begin(candidate, caret);
    const core::JwpPosition insertion =
        candidate.erase({selection_begin, selection_end});
    const core::JwpPosition following = candidate.insert_page_break(insertion);
    if (!history.commit(candidate, following)) {
      throw core::JwpDocumentEditError(
          "page-break insertion did not change the document");
    }

    std::u32string rendered =
        core::decode_jwp_plain_text(candidate, jwp_code_page_);
    const QString qt_text = to_qstring(rendered);
    const int qt_caret = utf16_offset_for_utf32(
        rendered, core::jwp_plain_text_offset(candidate, following));

    updating_editor_ = true;
    try {
      editor_->setPlainText(qt_text);
      apply_jwp_presentation(candidate.document(), jwp_code_page_);
      QTextCursor cursor(editor_->document());
      cursor.setPosition(qt_caret);
      editor_->setTextCursor(cursor);
    } catch (...) {
      editor_->setPlainText(original_text);
      apply_jwp_presentation(jwp_document_->document(), jwp_code_page_);
      editor_->setTextCursor(original_cursor);
      editor_->document()->setModified(original_modified);
      updating_editor_ = false;
      throw;
    }
    updating_editor_ = false;

    jwp_document_ = std::move(candidate);
    jwp_history_ = std::move(history);
    jwp_caret_ = following;
    expected_jwp_caret_.reset();
    rendered_jwp_text_ = std::move(rendered);
    const bool modified = !saved_jwp_document_.has_value() ||
                          jwp_document_->document() != *saved_jwp_document_;
    editor_->document()->setModified(modified);
    update_undo_actions();
    update_conversion_actions();
    update_title();
    statusBar()->showMessage(tr("Page break inserted"), 2000);
    return true;
  } catch (const std::exception& error) {
    updating_editor_ = false;
    statusBar()->showMessage(
        tr("Could not insert page break: %1")
            .arg(QString::fromUtf8(error.what())),
        5000);
    return false;
  }
}

void MainWindow::synchronize_jwp_document(int position, int chars_removed,
                                           int chars_added) {
  if (updating_editor_ || !jwp_document_.has_value()) {
    return;
  }

  if (automatic_conversion_range_.has_value() && !applying_kana_input_) {
    clear_automatic_conversion_range();
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

    const std::size_t fallback_caret_offset =
        chars_removed == 0 ? prefix : prefix + removed_length;
    const core::JwpPosition before_caret =
        jwp_caret_.has_value() &&
                jwp_document_->valid_position(*jwp_caret_)
            ? *jwp_caret_
            : core::jwp_plain_text_position(*jwp_document_,
                                            fallback_caret_offset);
    core::JwpDocumentModel updated = *jwp_document_;
    core::JwpDocumentHistory history = jwp_history_;
    history.begin(*jwp_document_, before_caret);
    const core::JwpPosition after_caret = core::replace_jwp_plain_text(
        updated, prefix, removed_length,
        std::u32string_view(current).substr(
            prefix, replacement_length),
        jwp_code_page_);
    const core::JwpHistoryKind kind =
        removed_length == 0 && replacement_length != 0
            ? core::JwpHistoryKind::kTyping
        : removed_length != 0 && replacement_length == 0
            ? core::JwpHistoryKind::kDeletion
            : core::JwpHistoryKind::kNone;
    history.commit(updated, after_caret, kind);
    const bool modified = !saved_jwp_document_.has_value() ||
                          updated.document() != *saved_jwp_document_;
    editor_->apply_kanji_colors(updated.document(), kanji_color_list_,
                                kanji_color_policy_, jwp_code_page_);
    jwp_document_ = std::move(updated);
    jwp_history_ = std::move(history);
    jwp_caret_ = after_caret;
    expected_jwp_caret_ = after_caret;
    rendered_jwp_text_ = std::move(current);
    editor_->document()->setModified(modified);
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
  const bool modified = saved_jwp_document_.has_value() &&
                        jwp_document_->document() != *saved_jwp_document_;
  updating_editor_ = true;
  editor_->undo();
  if (from_qstring(editor_->toPlainText()) != rendered_jwp_text_) {
    editor_->setPlainText(to_qstring(rendered_jwp_text_));
    apply_jwp_presentation(jwp_document_->document(), jwp_code_page_);
    QTextCursor cursor = editor_->textCursor();
    cursor.setPosition(
        std::min(cursor_position, editor_->document()->characterCount() - 1));
    editor_->setTextCursor(cursor);
  } else {
    editor_->apply_kanji_colors(jwp_document_->document(), kanji_color_list_,
                                kanji_color_policy_, jwp_code_page_);
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
  if (conversion_active() && !accept_conversion()) {
    return false;
  }
  finish_kana_input();
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
