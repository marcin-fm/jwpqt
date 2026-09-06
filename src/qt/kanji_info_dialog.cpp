// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_info_dialog.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QFont>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QTextBlock>
#include <QVBoxLayout>

#include "character_context_menu.h"
#include "jwpqt/core/jis_table.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/kana_input.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

QString hex_value(std::uint32_t value, int width = 4) {
  return QStringLiteral("%1").arg(value, width, 16, QLatin1Char('0')).toUpper();
}

QString decimal(std::uint32_t value) {
  return value == 0 ? QString() : QString::number(value);
}

QString paired(const QString& first, const QString& second) {
  if (first.isEmpty()) return second;
  if (second.isEmpty()) return first;
  return first + QStringLiteral("    ") + second;
}

QString skip_code(const core::KanjiInfoSkipCode& skip) {
  if (skip.type == 0) return {};
  return QStringLiteral("%1-%2-%3").arg(skip.type).arg(skip.first).arg(skip.second);
}

QString four_corner(std::uint16_t value, std::uint8_t index) {
  if (value == 0x3fffU) return {};
  return QStringLiteral("%1.%2").arg(value, 4, 10, QLatin1Char('0')).arg(index);
}

QString radical_symbol(unsigned radical) {
  if (radical == 0 || radical > 214) return {};
  return QString(QChar(static_cast<char16_t>(0x2f00 + radical - 1)))
      .normalized(QString::NormalizationForm_KC);
}

std::uint16_t reference_value(const core::KanjiInfoRecord& record, char kind) {
  std::uint16_t value = 0;
  for (const auto& reference : record.references)
    if (reference.kind == kind) value = reference.value;
  return value;
}

}  // namespace

KanjiInfoDialog::KanjiInfoDialog(
    const core::KanjiInfoDatabase* database,
    std::function<void(char32_t)> show_information, QWidget* parent,
    std::function<bool(std::u32string)> insert)
    : QDialog(parent), database_(database),
      show_information_(std::move(show_information)), insert_(std::move(insert)),
      character_(new QLabel(this)), status_(new QLabel(this)),
      fields_(new QTableWidget(this)), readings_(new QTextEdit(this)),
      insert_button_(new QPushButton(tr("&Insert to File"), this)),
      more_button_(new QPushButton(tr("&More Info"), this)) {
  setObjectName(QStringLiteral("kanjiInfoDialog"));
  setWindowTitle(tr("Character Information"));
  setModal(false);
  resize(940, 560);

  auto* outer = new QVBoxLayout(this);
  auto* columns = new QHBoxLayout;
  auto* left = new QVBoxLayout;
  character_->setObjectName(QStringLiteral("kanjiInfoCharacter"));
  character_->setTextFormat(Qt::PlainText);
  character_->setAlignment(Qt::AlignCenter);
  character_->setMinimumSize(200, 200);
  character_->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  character_->setBackgroundRole(QPalette::Base);
  character_->setForegroundRole(QPalette::Text);
  character_->setAutoFillBackground(true);
  QFont character_font = character_->font();
  character_font.setPointSize(112);
  character_->setFont(character_font);
  character_->setToolTip(tr("Double-click to insert this character into the file"));
  character_->installEventFilter(this);
  left->addWidget(character_);
  left->addSpacing(12);

  auto* clipboard = new QPushButton(tr("From &Clipboard"), this);
  clipboard->setObjectName(QStringLiteral("kanjiInfoClipboard"));
  connect(clipboard, &QPushButton::clicked, this, [this] {
    const auto text = from_qstring(QApplication::clipboard()->text());
    if (text.empty() || !set_character(text.front(), code_page_))
      status_->setText(tr("The clipboard does not start with a displayable character."));
  });
  insert_button_->setObjectName(QStringLiteral("kanjiInfoInsert"));
  insert_button_->setEnabled(false);
  insert_button_->setToolTip(tr("Insert selected text from the readings pane"));
  connect(insert_button_, &QPushButton::clicked, this, [this] {
    insert_text(readings_->textCursor().selectedText());
  });
  more_button_->setObjectName(QStringLiteral("kanjiInfoMore"));
  more_button_->setEnabled(false);
  connect(more_button_, &QPushButton::clicked, this, &KanjiInfoDialog::show_more_info);
  auto* done = new QPushButton(tr("&Done"), this);
  done->setObjectName(QStringLiteral("kanjiInfoDone"));
  done->setDefault(true);
  connect(done, &QPushButton::clicked, this, &QDialog::close);
  for (auto* button : {clipboard, insert_button_, more_button_, done}) {
    button->setAutoDefault(false);
    left->addWidget(button);
  }
  left->addStretch();
  columns->addLayout(left);

  fields_->setObjectName(QStringLiteral("kanjiInfoFields"));
  fields_->setColumnCount(2);
  fields_->horizontalHeader()->hide();
  fields_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  fields_->horizontalHeader()->setStretchLastSection(true);
  fields_->verticalHeader()->hide();
  fields_->setShowGrid(false);
  fields_->setFrameShape(QFrame::NoFrame);
  fields_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  fields_->setSelectionMode(QAbstractItemView::NoSelection);
  fields_->setFocusPolicy(Qt::NoFocus);
  fields_->setMinimumWidth(300);
  columns->addWidget(fields_, 1);

  readings_->setObjectName(QStringLiteral("kanjiInfoReadings"));
  readings_->setReadOnly(true);
  readings_->setUndoRedoEnabled(false);
  QFont reading_font = readings_->font();
  reading_font.setPointSizeF(std::max(12.0, reading_font.pointSizeF()));
  readings_->setFont(reading_font);
  readings_->setMinimumWidth(260);
  readings_->viewport()->installEventFilter(this);
  readings_->installEventFilter(this);
  connect(readings_, &QTextEdit::copyAvailable, this, [this](bool selected) {
    insert_button_->setEnabled(selected && static_cast<bool>(insert_));
  });
  columns->addWidget(readings_, 1);
  outer->addLayout(columns, 1);
  status_->setObjectName(QStringLiteral("kanjiInfoStatus"));
  status_->setTextFormat(Qt::PlainText);
  status_->setWordWrap(true);
  outer->addWidget(status_);
}

bool KanjiInfoDialog::set_code(core::JisCode code, core::LegacyCodePage code_page) {
  try {
    const auto text = core::decode_jwp_text({code}, code_page);
    return text.size() == 1 && populate(text.front(), code, code_page);
  } catch (const core::JwpTextCodecError&) {
    return false;
  }
}

bool KanjiInfoDialog::set_character(char32_t character,
                                  core::LegacyCodePage code_page) {
  std::optional<core::JisCode> code;
  try {
    const auto encoded = core::encode_jwp_text(std::u32string{character}, code_page);
    if (encoded.size() == 1) code = encoded.front();
  } catch (const core::JwpTextCodecError&) {
    // Unicode text can still be inspected when it is outside the JWP repertoire.
  }
  return populate(character, code, code_page);
}

bool KanjiInfoDialog::populate(char32_t character, std::optional<core::JisCode> code,
                               core::LegacyCodePage code_page) {
  if (character > 0x10ffffU || !QChar::isPrint(character)) return false;
  std::optional<core::KanjiInfoRecord> record;
  QString metadata_error;
  if (code && database_ != nullptr && database_->contains(*code)) {
    try {
      record = database_->record(*code);
    } catch (const core::KanjiInfoError& error) {
      metadata_error = QString::fromUtf8(error.what());
    } catch (const core::JwpTextCodecError& error) {
      metadata_error = QString::fromUtf8(error.what());
    }
  }
  if (more_dialog_ != nullptr) more_dialog_->hide();
  unicode_ = character;
  code_ = code.value_or(0);
  code_page_ = code_page;
  character_->setText(to_qstring(std::u32string{character}));
  populate_fields(record ? &*record : nullptr);
  populate_readings(record ? &*record : nullptr);
  more_button_->setEnabled(record.has_value());
  status_->setText(!metadata_error.isEmpty()
      ? tr("Could not read kanji metadata: %1. Basic character codes are shown.")
            .arg(metadata_error)
      : !record && code_ >= 0x3021
      ? tr("Kanji metadata is unavailable; basic character codes are shown.")
      : tr("Right-click a character for information. Select reading text to insert it."));
  setWindowTitle(tr("Character Information - %1").arg(character_->text()));
  return true;
}

core::JisCode KanjiInfoDialog::code() const noexcept { return code_; }
char32_t KanjiInfoDialog::character() const noexcept { return unicode_; }

void KanjiInfoDialog::populate_fields(const core::KanjiInfoRecord* record) {
  QString type = tr("Unicode character");
  if (code_ > 0 && code_ <= 0x7f) type = tr("ASCII");
  else if (code_ > 0 && code_ <= 0xff) type = tr("Extended code-page character");
  else if (code_ >= 0x3021 && code_ < 0x5000) type = tr("Kanji (common, I)");
  else if (code_ >= 0x5000) type = tr("Kanji (uncommon, II)");
  else {
    switch (code_ >> 8U) {
      case 0x21: case 0x22: type = tr("Japanese symbol"); break;
      case 0x23: type = tr("Japanese ASCII"); break;
      case 0x24: type = tr("Hiragana"); break;
      case 0x25: type = tr("Katakana"); break;
      case 0x26: type = tr("Greek"); break;
      case 0x27: type = tr("Cyrillic"); break;
      case 0x28: type = tr("Box drawing"); break;
    }
  }
  std::vector<std::pair<QString, QString>> rows{{tr("Type"), type}};
  if (const auto entry = core::describe_jis_character(code_)) {
    rows.emplace_back(tr("JIS Code"), QStringLiteral("%1 (%2)")
        .arg(hex_value(code_), hex_value(code_ | 0x8080U)));
    rows.emplace_back(tr("Shift-JIS"), hex_value(
        (entry->shift_jis.lead << 8U) | entry->shift_jis.trail));
  } else if (code_ != 0) {
    rows.emplace_back(tr("Byte code"), hex_value(code_, 2));
  }
  rows.emplace_back(tr("Unicode"), QStringLiteral("U+%1").arg(hex_value(unicode_)));
  more_info_.clear();
  if (record != nullptr) {
    const auto& fixed = record->fixed;
    const auto& extended = record->extended;
    rows.emplace_back(tr("Strokes"), decimal(fixed.strokes));
    QString bushu = decimal(fixed.bushu);
    if (fixed.classical_bushu != 0)
      bushu += QStringLiteral(" (%1)").arg(fixed.classical_bushu);
    rows.emplace_back(tr("Bushu"), paired(bushu,
        paired(radical_symbol(fixed.bushu), radical_symbol(fixed.classical_bushu))));
    rows.emplace_back(tr("Grade"), decimal(fixed.grade));
    rows.emplace_back(tr("Frequency"), decimal(reference_value(*record, 'F')));
    rows.emplace_back(tr("Halpern / SKIP"), paired(decimal(fixed.halpern),
                                                  skip_code(fixed.skip)));
    QString spahn;
    if (record->has_extended &&
        (extended.spahn_radical_strokes != 0 || extended.spahn_other_strokes != 0))
      spahn = QStringLiteral("%1%2%3.%4").arg(extended.spahn_radical_strokes)
          .arg(QChar(static_cast<char16_t>('a' + extended.spahn_radical)))
          .arg(extended.spahn_other_strokes).arg(extended.spahn_index);
    rows.emplace_back(tr("Spahn"), paired(spahn, decimal(reference_value(*record, 'I'))));
    QString corner;
    QString morohashi;
    if (record->has_extended) {
      corner = four_corner(extended.four_corner, extended.four_corner_index);
      const auto secondary = reference_value(*record, 'Q');
      if (secondary != 0)
        corner = paired(corner, four_corner(secondary, extended.four_corner_second_index));
      if (extended.morohashi_long != 0) {
        morohashi = decimal(extended.morohashi_long);
        if (extended.morohashi_cross) morohashi += QLatin1Char('X');
        else if (extended.morohashi_page) morohashi += QLatin1Char('P');
      }
      if (extended.morohashi_volume != 0)
        morohashi = paired(morohashi, QStringLiteral("%1.%2")
            .arg(extended.morohashi_volume)
            .arg(extended.morohashi_index, 4, 10, QLatin1Char('0')));
    }
    rows.emplace_back(tr("Four Corners"), corner);
    rows.emplace_back(tr("Morohashi"), morohashi);
    rows.emplace_back(tr("Pinyin"), to_qstring(record->pinyin));
    rows.emplace_back(tr("Korean"), to_qstring(record->korean));
    rows.emplace_back(tr("Nelson"), paired(decimal(fixed.nelson), decimal(fixed.haig)));

    QStringList extra;
    for (const auto& reference : record->references) {
      QString label;
      QString value = QString::number(reference.value);
      switch (reference.kind) {
        case 'F': label = tr("Frequency"); break;
        case 'I': label = tr("Spahn kana index"); break;
        case 'B': label = tr("Japanese for Busy People");
          value = QStringLiteral("%1.%2").arg(reference.value >> 8U)
              .arg(reference.value & 0xffU); break;
        case 'C': label = tr("The Kanji Way"); break;
        case 'D': label = tr("De Roo"); break;
        case 'E': label = tr("Henshall"); break;
        case 'G': label = tr("Kodansha Compact Kanji Guide"); break;
        case 'H': label = tr("Halpern Kanji Learners Dictionary"); break;
        case 'J': label = tr("Kanji in Context"); break;
        case 'K': label = tr("Gakken"); break;
        case 'L': label = tr("Heisig"); break;
        case 'N': label = tr("O'Neill Essential Kanji"); break;
        case 'O': label = tr("O'Neill Japanese Names"); break;
        case 'S': label = tr("A Guide to Reading and Writing Japanese"); break;
        case 'T': label = tr("Tuttle Kanji Cards"); break;
        case 'Q': label = tr("Secondary Four Corners");
          value = four_corner(reference.value, extended.four_corner_second_index); break;
        case 'n': label = tr("Alternate Nelson"); break;
        case 'h': label = tr("Alternate Halpern"); break;
        case 'o': label = tr("Alternate O'Neill"); break;
        case 'd': label = tr("Alternate De Roo"); break;
        case 'k': label = tr("JIS X 0208 reference");
          value = hex_value(reference.value);
          if (const auto entry = core::describe_jis_character(reference.value))
            value += QStringLiteral("  ") + to_qstring(std::u32string{entry->unicode});
          break;
        case 'j': label = tr("JIS X 0212 reference");
          value = hex_value(reference.value); break;
        case 'z': {
          label = tr("Alternate SKIP");
          const unsigned bits = reference.value;
          const QStringList kinds{tr("Unspecified"), tr("Position"), tr("Stroke"),
                                  tr("Position and stroke"), tr("Breen")};
          const auto kind = bits >> 13U;
          value = QStringLiteral("%1-%2-%3 (%4)").arg((bits >> 10U) & 7U)
              .arg((bits >> 5U) & 31U).arg(bits & 31U)
              .arg(kinds[kind < static_cast<unsigned>(kinds.size()) ? kind : 0]);
          break;
        }
        default: label = tr("Reference %1").arg(QChar::fromLatin1(reference.kind));
      }
      extra.append(label + QStringLiteral(": ") + value);
    }
    more_info_ = extra.isEmpty() ? tr("No additional references are recorded.")
                                 : extra.join(QLatin1Char('\n'));
  }
  fields_->setRowCount(static_cast<int>(rows.size()));
  for (std::size_t row = 0; row < rows.size(); ++row) {
    fields_->setItem(static_cast<int>(row), 0, new QTableWidgetItem(rows[row].first));
    fields_->setItem(static_cast<int>(row), 1, new QTableWidgetItem(rows[row].second));
  }
  fields_->resizeRowsToContents();
}

void KanjiInfoDialog::populate_readings(const core::KanjiInfoRecord* record) {
  readings_->clear();
  QTextCursor cursor(readings_->document());
  const auto append = [&](const QString& text, bool heading = false) {
    if (!cursor.atStart()) cursor.insertBlock();
    QTextCharFormat format;
    if (heading) {
      format.setFontWeight(QFont::Bold);
      format.setProperty(QTextFormat::UserProperty, true);
    }
    cursor.insertText(text, format);
  };
  if (record != nullptr) {
    const auto section = [&](const QString& title,
                             const std::vector<std::u32string>& entries) {
      if (entries.empty()) return;
      append(title, true);
      for (const auto& text : entries) append(to_qstring(text));
    };
    section(tr("-- meanings --"), record->meanings);
    section(tr("-- on-yomi --"), record->on_readings);
    section(tr("-- kun-yomi --"), record->kun_readings);
    section(tr("-- nanori --"), record->nanori);
  } else {
    const auto spellings = core::kana_input_spellings(code_);
    if (!spellings.empty()) {
      append(tr("-- romaji --"), true);
      for (const auto spelling : spellings)
        append(QString::fromLatin1(spelling.data(), static_cast<qsizetype>(spelling.size())));
    } else if ((code_ >= 0x2621 && code_ <= 0x2638) ||
               (code_ >= 0x2641 && code_ <= 0x2658)) {
      static constexpr const char* names[]{
          "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta",
          "iota", "kappa", "lambda", "mu", "nu", "xi", "omicron", "pi", "rho",
          "sigma", "tau", "upsilon", "phi", "chi", "psi", "omega"};
      append(QString::fromLatin1(names[(code_ & 0xffU) -
                                      ((code_ & 0xffU) >= 0x41 ? 0x41 : 0x21)]));
    } else if ((code_ >= 0x2721 && code_ <= 0x2741) ||
               (code_ >= 0x2751 && code_ <= 0x2771)) {
      static constexpr const char* names[]{
          "a", "be", "ve", "ge / ghe", "de", "ye / ie", "yo / io", "zhe", "ze",
          "i", "short i", "ka", "el", "em", "en", "o", "pe", "er", "es", "te",
          "u", "ef", "kha / ha", "tse", "che", "sha", "shcha", "yer / hard sign",
          "yery / yeru", "yeri / soft sign", "e", "yu", "ya"};
      append(QString::fromLatin1(names[(code_ & 0xffU) -
                                      ((code_ & 0xffU) >= 0x51 ? 0x51 : 0x21)]));
    }
  }
  readings_->moveCursor(QTextCursor::Start);
  update_reading_colors();
}

void KanjiInfoDialog::update_reading_colors() {
  const auto luminance = [](const QColor& color) {
    const auto linear = [](qreal value) {
      return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) +
           0.0722 * linear(color.blueF());
  };
  const auto background = luminance(readings_->palette().color(QPalette::Base));
  QColor color;
  for (const QColor& candidate : {QColor(QStringLiteral("#b00020")),
                                  QColor(QStringLiteral("#ff8080")),
                                  readings_->palette().color(QPalette::Text),
                                  QColor(Qt::black), QColor(Qt::white)}) {
    const auto foreground = luminance(candidate);
    if ((std::max(background, foreground) + 0.05) /
        (std::min(background, foreground) + 0.05) >= 4.5) {
      color = candidate;
      break;
    }
  }
  for (auto block = readings_->document()->begin(); block.isValid(); block = block.next()) {
    if (block.length() <= 1) continue;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    if (!cursor.charFormat().property(QTextFormat::UserProperty).toBool()) continue;
    cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
    QTextCharFormat format;
    format.setForeground(color);
    cursor.mergeCharFormat(format);
  }
}

void KanjiInfoDialog::insert_text(const QString& text) {
  if (text.isEmpty() || !insert_) return;
  QString plain = text;
  plain.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
  if (!insert_(from_qstring(plain)))
    status_->setText(tr("The text could not be inserted into the current document."));
  else
    status_->setText(tr("Inserted into the file; Undo is available in the editor."));
}

void KanjiInfoDialog::show_more_info() {
  if (!more_button_->isEnabled()) return;
  if (more_dialog_ == nullptr) {
    more_dialog_ = new QDialog(this);
    more_dialog_->setObjectName(QStringLiteral("kanjiInfoMoreDialog"));
    more_dialog_->resize(500, 420);
    auto* layout = new QVBoxLayout(more_dialog_);
    more_text_ = new QTextEdit(more_dialog_);
    more_text_->setObjectName(QStringLiteral("kanjiInfoReferences"));
    more_text_->setReadOnly(true);
    more_text_->setUndoRedoEnabled(false);
    more_text_->viewport()->installEventFilter(this);
    more_text_->installEventFilter(this);
    layout->addWidget(more_text_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, more_dialog_);
    auto* insert = buttons->addButton(tr("Insert to File"), QDialogButtonBox::ActionRole);
    insert->setObjectName(QStringLiteral("kanjiInfoReferenceInsert"));
    insert->setEnabled(false);
    connect(more_text_, &QTextEdit::copyAvailable, insert, [this, insert](bool selected) {
      insert->setEnabled(selected && static_cast<bool>(insert_));
    });
    connect(insert, &QPushButton::clicked, this, [this] {
      insert_text(more_text_->textCursor().selectedText());
    });
    connect(buttons, &QDialogButtonBox::rejected, more_dialog_, &QDialog::close);
    layout->addWidget(buttons);
  }
  more_text_->setPlainText(more_info_);
  more_dialog_->setWindowTitle(tr("More Character Information - %1").arg(character_->text()));
  more_dialog_->show();
  more_dialog_->raise();
  more_dialog_->activateWindow();
}

bool KanjiInfoDialog::eventFilter(QObject* watched, QEvent* event) {
  if (watched == readings_ && event->type() == QEvent::PaletteChange)
    update_reading_colors();
  if (event->type() == QEvent::MouseButtonPress &&
      static_cast<QMouseEvent*>(event)->button() == Qt::RightButton &&
      (watched == readings_->viewport() ||
       (more_text_ != nullptr && watched == more_text_->viewport()))) return true;
  if (event->type() == QEvent::ContextMenu) {
    QTextEdit* source = watched == readings_ || watched == readings_->viewport() ? readings_ :
        more_text_ != nullptr && (watched == more_text_ || watched == more_text_->viewport())
            ? more_text_ : nullptr;
    if (source != nullptr) {
      show_character_context_menu(*source, *static_cast<QContextMenuEvent*>(event),
          [this](CharacterTarget target) {
            show_information_(target.character);
          });
      return true;
    }
  }
  if (watched == character_ && event->type() == QEvent::MouseButtonDblClick &&
      static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
    insert_text(character_->text());
    return true;
  }
  if (watched == readings_ && event->type() == QEvent::KeyPress &&
      (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Enter) &&
      !(static_cast<QKeyEvent*>(event)->modifiers() &
        (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) &&
      readings_->textCursor().hasSelection()) {
    insert_text(readings_->textCursor().selectedText());
    return true;
  }
  return QDialog::eventFilter(watched, event);
}

}  // namespace jwpqt::qt
