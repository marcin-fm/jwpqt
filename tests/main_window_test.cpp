// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QLabel>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextBlockFormat>

#include "file_io.h"
#include "jwp_editor.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "main_window.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

QByteArray read_bytes(const QString& path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Could not open saved test file");
  return file.readAll();
}

void write_bytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
          "Could not create binary test file");
  require(file.write(bytes) == bytes.size(),
          "Could not write complete binary test file");
}

struct WnnFixture {
  QString index_path;
  QString data_path;
  QString preferences_path;
};

WnnFixture write_wnn_fixture(const QString& directory) {
  const WnnFixture fixture{
      directory + QStringLiteral("/wnn.dix"),
      directory + QStringLiteral("/wnn.dat"),
      directory + QStringLiteral("/user.sel"),
  };
  QByteArray index;
  index.append(static_cast<char>(0xa2));
  index.append(static_cast<char>(0x80));
  index.append(static_cast<char>(0x80));
  index.append(static_cast<char>(0x77));
  index.append(4, '\0');
  QByteArray data;
  data.append(static_cast<char>(0xa2));
  data.append('*');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa1));
  data.append('/');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa2));
  data.append('\n');
  write_bytes(fixture.index_path, index);
  write_bytes(fixture.data_path, data);
  return fixture;
}

void append_u32_le(QByteArray& bytes, quint32 value) {
  for (int shift = 0; shift < 32; shift += 8) {
    bytes.append(static_cast<char>((value >> shift) & 0xffU));
  }
}

void append_wnn_index_entry(QByteArray& index, unsigned char first,
                            unsigned char second, quint32 offset) {
  index.append(static_cast<char>(first));
  index.append(static_cast<char>(second));
  index.append(static_cast<char>(0x80));
  index.append(static_cast<char>(0x77));
  append_u32_le(index, offset);
}

WnnFixture write_automatic_wnn_fixture(const QString& directory) {
  const WnnFixture fixture{
      directory + QStringLiteral("/automatic.dix"),
      directory + QStringLiteral("/automatic.dat"),
      directory + QStringLiteral("/automatic-user.sel"),
  };
  QByteArray data;
  data.append(static_cast<char>(0xab));
  data.append('*');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa1));
  data.append('/');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa2));
  data.append('\n');
  const quint32 second_offset = static_cast<quint32>(data.size());
  data.append(static_cast<char>(0xab));
  data.append(static_cast<char>(0xad));
  data.append('*');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa3));
  data.append('/');
  data.append(static_cast<char>(0xb0));
  data.append(static_cast<char>(0xa4));
  data.append('\n');

  QByteArray index;
  append_wnn_index_entry(index, 0xab, 0x80, 0);
  append_wnn_index_entry(index, 0xab, 0xad, second_offset);
  write_bytes(fixture.index_path, index);
  write_bytes(fixture.data_path, data);
  return fixture;
}

class PromptingWindow : public jwpqt::qt::MainWindow {
 public:
  std::optional<jwpqt::core::TextEncoding> next_encoding;
  std::optional<jwpqt::qt::SearchRequest> next_search;
  std::optional<jwpqt::qt::ReplaceRequest> next_replace;
  std::optional<jwpqt::core::JwpParagraphFormat> next_paragraph_format;
  std::optional<jwpqt::core::JwpParagraphFormat> offered_paragraph_format;
  std::vector<jwpqt::core::TextEncoding> offered_encodings;
  QString explanation;
  int search_prompt_count = 0;
  int replace_prompt_count = 0;
  int paragraph_format_prompt_count = 0;

 protected:
  std::optional<jwpqt::core::TextEncoding> prompt_for_encoding(
      const std::vector<jwpqt::core::TextEncoding>& candidates,
      const QString& prompt) override {
    offered_encodings = candidates;
    explanation = prompt;
    return next_encoding;
  }

  std::optional<jwpqt::qt::SearchRequest> prompt_for_search(
      const jwpqt::qt::SearchRequest&) override {
    ++search_prompt_count;
    return next_search;
  }

  std::optional<jwpqt::qt::ReplaceRequest> prompt_for_replace(
      const jwpqt::qt::ReplaceRequest&) override {
    ++replace_prompt_count;
    return next_replace;
  }

  std::optional<jwpqt::core::JwpParagraphFormat>
  prompt_for_paragraph_format(
      const jwpqt::core::JwpParagraphFormat& initial) override {
    ++paragraph_format_prompt_count;
    offered_paragraph_format = initial;
    return next_paragraph_format;
  }
};

QAction* find_encoding_action(jwpqt::qt::MainWindow& window,
                              const QString& name) {
  for (QAction* action : window.findChildren<QAction*>()) {
    if (action->text() == name) {
      return action;
    }
  }
  return nullptr;
}

QAction* find_action(jwpqt::qt::MainWindow& window, const char* name) {
  return window.findChild<QAction*>(QString::fromLatin1(name));
}

jwpqt::core::JwpParagraph paragraph(std::u32string_view text,
                                    std::int16_t line_spacing = 100) {
  jwpqt::core::JwpParagraph result;
  result.text = jwpqt::core::encode_jwp_text(text);
  result.line_spacing = line_spacing;
  return result;
}

jwpqt::core::JwpDocument sample_jwp_document() {
  jwpqt::core::JwpDocument document;
  document.margins = {1.0F, 1.25F, 1.5F, 1.75F};
  document.landscape = true;
  document.summary[0] = jwpqt::core::encode_jwp_text(U"Sample title");
  document.headers[0][0] = jwpqt::core::encode_jwp_text(U"Header");
  document.paragraphs = {paragraph(U"A\u65e5\u672c", 125),
                         paragraph(U"\u00e9", 150)};
  document.paragraphs[0].first_indent = -2;
  document.paragraphs[0].left_indent = 3;
  document.paragraphs[0].right_indent = 4;
  return document;
}

void require_jwp_layout(QTextEdit* editor,
                        const jwpqt::core::JwpDocument& document) {
  require(editor != nullptr &&
              editor->document()->blockCount() ==
                  static_cast<int>(document.paragraphs.size()),
          "JWP editor layout has the wrong paragraph count");
  const QFontMetricsF metrics(editor->font());
  const qreal ideographic_space = metrics.horizontalAdvance(QChar(0x3000));
  const qreal unit =
      ideographic_space > 0.0 ? ideographic_space : metrics.height();
  QTextBlock block = editor->document()->begin();
  bool follows_page_break = false;
  for (const jwpqt::core::JwpParagraph& paragraph : document.paragraphs) {
    const QTextBlockFormat format = block.blockFormat();
    require(std::abs(format.leftMargin() - paragraph.left_indent * unit) <
                    0.01 &&
                std::abs(format.rightMargin() - paragraph.right_indent * unit) <
                    0.01 &&
                std::abs(format.textIndent() - paragraph.first_indent * unit) <
                    0.01,
            "JWP editor did not retain paragraph indents");
    require(format.lineHeightType() ==
                    QTextBlockFormat::ProportionalHeight &&
                std::abs(format.lineHeight() -
                         std::max<std::int16_t>(paragraph.line_spacing, 1)) <
                    0.01,
            "JWP editor did not retain paragraph line spacing");
    require(format.property(jwpqt::qt::JwpEditor::kPageBreakProperty)
                    .toBool() == paragraph.page_break,
            "JWP editor did not retain hard-page-break metadata");
    const bool starts_new_page =
        (format.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore) != 0;
    require(starts_new_page == follows_page_break,
            "JWP editor did not retain hard-page-break layout policy");
    follows_page_break = paragraph.page_break;
    block = block.next();
  }
}

void test_explicit_open_and_encoding_action(const QString& directory) {
  const QString path = directory + QStringLiteral("/explicit.euc");
  const jwpqt::core::TextFile file{
      U"ASCII \u65e5\u672c\u8a9e\n", jwpqt::core::TextEncoding::kEucJp,
      false};
  jwpqt::qt::write_text_file(path, file);

  jwpqt::qt::MainWindow window;
  require(window.open_path(path, jwpqt::core::TextEncoding::kEucJp),
          "Could not explicitly open EUC-JP file");
  require(window.text_encoding() == jwpqt::core::TextEncoding::kEucJp,
          "Window did not retain EUC-JP encoding");

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Window has no editor");
  require(editor->toPlainText() == QStringLiteral("ASCII \u65e5\u672c\u8a9e\n"),
          "Window decoded the EUC-JP text incorrectly");
  require(!editor->document()->isModified(),
          "Opening a document marked it modified");

  QLabel* label = window.findChild<QLabel*>(QStringLiteral("documentEncoding"));
  require(label != nullptr && label->text() == QStringLiteral("EUC-JP"),
          "Encoding status did not show EUC-JP");

  QAction* shift_jis_action =
      find_encoding_action(window, QStringLiteral("Shift-JIS"));
  require(shift_jis_action != nullptr, "Shift-JIS action was not created");
  shift_jis_action->trigger();
  require(window.text_encoding() == jwpqt::core::TextEncoding::kShiftJis,
          "Encoding action did not select Shift-JIS");
  require(editor->document()->isModified(),
          "Changing the save encoding did not mark the document modified");
  require(label->text() == QStringLiteral("Shift-JIS"),
          "Encoding status did not update to Shift-JIS");

  const QString saved_path = directory + QStringLiteral("/saved.sjis");
  require(window.save_path(saved_path), "Could not save as Shift-JIS");
  require(read_bytes(saved_path) ==
              QByteArray::fromHex("41534349492093fa967b8cea0a"),
          "Encoding selection did not control saved bytes");
  require(!editor->document()->isModified(),
          "Successful explicit save did not clear modified state");

  QAction* old_jis_action =
      find_encoding_action(window, QStringLiteral("Old JIS"));
  require(old_jis_action != nullptr, "Old JIS action was not created");
  old_jis_action->trigger();
  const QString old_jis_path = directory + QStringLiteral("/saved.old");
  require(window.save_path(old_jis_path), "Could not save as Old JIS");
  require(read_bytes(old_jis_path) ==
              QByteArray::fromHex(
                  "4153434949201b2440467c4b5c386c1b284a0a"),
          "Old JIS action did not control saved bytes");
}

void test_leaving_utf8_drops_bom(const QString& directory) {
  const QString source_path = directory + QStringLiteral("/bom.txt");
  jwpqt::qt::write_text_file(
      source_path, {U"ASCII", jwpqt::core::TextEncoding::kUtf8, true});

  jwpqt::qt::MainWindow window;
  require(window.open_path(source_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open UTF-8 BOM file");
  QAction* euc_action =
      find_encoding_action(window, QStringLiteral("EUC-JP"));
  QAction* utf8_action =
      find_encoding_action(window, QStringLiteral("UTF-8"));
  require(euc_action != nullptr && utf8_action != nullptr,
          "Encoding actions were not created");
  euc_action->trigger();
  utf8_action->trigger();

  const QString saved_path = directory + QStringLiteral("/without-bom.txt");
  require(window.save_path(saved_path), "Could not save switched UTF-8 file");
  require(read_bytes(saved_path) == QByteArray("ASCII"),
          "Switching away from UTF-8 did not clear BOM metadata");
}

void test_detected_open(const QString& directory) {
  const QString path = directory + QStringLiteral("/detected.euc");
  jwpqt::qt::write_text_file(
      path, {U"ASCII \u65e5\u672c\u8a9e\n",
             jwpqt::core::TextEncoding::kEucJp, false});

  jwpqt::qt::MainWindow window;
  require(window.open_path_detected(path),
          "Could not open a certainly detected EUC-JP file");
  require(window.text_encoding() == jwpqt::core::TextEncoding::kEucJp,
          "Detected open did not retain EUC-JP");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr &&
              editor->toPlainText() == QStringLiteral("ASCII \u65e5\u672c\u8a9e\n"),
          "Detected open decoded EUC-JP incorrectly");
}

void test_detected_bom_is_preserved(const QString& directory) {
  const QString source_path = directory + QStringLiteral("/detected-bom.txt");
  jwpqt::qt::write_text_file(
      source_path, {U"\u65e5\u672c\u8a9e", jwpqt::core::TextEncoding::kUtf8,
                    true});

  jwpqt::qt::MainWindow window;
  require(window.open_path_detected(source_path),
          "Could not detect UTF-8 BOM file");
  const QString saved_path = directory + QStringLiteral("/preserved-bom.txt");
  require(window.save_path(saved_path), "Could not save detected BOM file");
  require(read_bytes(saved_path).startsWith(QByteArray::fromHex("efbbbf")),
          "Detected UTF-8 BOM was not preserved");
}

void test_detection_prompt_and_cancellation(const QString& directory) {
  const QString initial_path = directory + QStringLiteral("/initial.txt");
  jwpqt::qt::write_text_file(
      initial_path,
      {U"original", jwpqt::core::TextEncoding::kUtf8, false});
  const QString ambiguous_path = directory + QStringLiteral("/ambiguous.bin");
  QFile ambiguous(ambiguous_path);
  require(ambiguous.open(QIODevice::WriteOnly),
          "Could not create ambiguous fixture");
  require(ambiguous.write(QByteArray::fromHex("e0a1")) == 2,
          "Could not write ambiguous fixture");
  ambiguous.close();

  PromptingWindow window;
  require(window.open_path(initial_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open initial prompt-state document");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Prompting window has no editor");
  window.next_encoding = std::nullopt;
  require(!window.open_path_detected(ambiguous_path),
          "Cancelled ambiguous detection unexpectedly opened");
  require(editor->toPlainText() == QStringLiteral("original") &&
              window.text_encoding() == jwpqt::core::TextEncoding::kUtf8,
          "Cancelled detection changed current document state");
  require(window.offered_encodings ==
              std::vector<jwpqt::core::TextEncoding>{
                  jwpqt::core::TextEncoding::kEucJp,
                  jwpqt::core::TextEncoding::kShiftJis},
          "Ambiguous detection did not offer exact viable encodings");

  window.next_encoding = jwpqt::core::TextEncoding::kEucJp;
  require(window.open_path_detected(ambiguous_path),
          "Selected ambiguous encoding did not open");
  require(window.text_encoding() == jwpqt::core::TextEncoding::kEucJp,
          "Selected ambiguous encoding was not retained");
  const QString saved_path = directory + QStringLiteral("/ambiguous.euc");
  require(window.save_path(saved_path), "Could not save ambiguous selection");
  require(read_bytes(saved_path) == QByteArray::fromHex("e0a1"),
          "Ambiguous selected encoding did not preserve canonical bytes");
}

void test_ascii_and_unknown_prompts(const QString& directory) {
  const QString ascii_path = directory + QStringLiteral("/ascii.txt");
  QFile ascii(ascii_path);
  require(ascii.open(QIODevice::WriteOnly), "Could not create ASCII fixture");
  require(ascii.write("ASCII") == 5, "Could not write ASCII fixture");
  ascii.close();

  PromptingWindow window;
  window.next_encoding = jwpqt::core::TextEncoding::kShiftJis;
  require(window.open_path_detected(ascii_path),
          "Selected ASCII encoding did not open");
  require(window.offered_encodings.size() == 6 &&
              window.text_encoding() ==
                  jwpqt::core::TextEncoding::kShiftJis,
          "ASCII prompt did not expose and retain explicit encoding");

  const QString invalid_path = directory + QStringLiteral("/invalid.bin");
  QFile invalid(invalid_path);
  require(invalid.open(QIODevice::WriteOnly),
          "Could not create invalid fixture");
  require(invalid.write(QByteArray::fromHex("ff")) == 1,
          "Could not write invalid fixture");
  invalid.close();
  window.next_encoding = std::nullopt;
  require(!window.open_path_detected(invalid_path),
          "Cancelled unknown detection unexpectedly opened");
  require(window.offered_encodings.empty(),
          "Unknown detection unexpectedly constrained encoding choices");
}

void test_plain_text_find_actions(const QString& directory) {
  const QString path = directory + QStringLiteral("/find.txt");
  jwpqt::qt::write_text_file(
      path, jwpqt::core::TextFile{U"x Alpha alpha \u00c9 \u00e9",
                                 jwpqt::core::TextEncoding::kUtf8, false});

  PromptingWindow window;
  require(window.open_path(path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open plain search fixture");
  window.next_search = jwpqt::qt::SearchRequest{
      QStringLiteral("ALPHA"), jwpqt::core::JwpSearchOptions{}};
  QAction* find = find_action(window, "findAction");
  QAction* find_next = find_action(window, "findNextAction");
  QAction* find_previous = find_action(window, "findPreviousAction");
  require(find != nullptr && find_next != nullptr && find_previous != nullptr,
          "Native find actions were not created");

  find->trigger();
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(window.search_prompt_count == 1 && editor != nullptr &&
              editor->textCursor().selectedText() == QStringLiteral("Alpha"),
          "Find dialog action did not select the first plain-text match");
  find_next->trigger();
  require(editor->textCursor().selectedText() == QStringLiteral("alpha"),
          "Find Next did not select the following plain-text match");
  find_previous->trigger();
  require(editor->textCursor().selectedText() == QStringLiteral("Alpha"),
          "Find Previous did not select the preceding plain-text match");

  const QTextCursor original = editor->textCursor();
  require(!window.find_text(QStringLiteral("missing")),
          "Missing plain text unexpectedly matched");
  require(editor->textCursor().selectionStart() == original.selectionStart() &&
              editor->textCursor().selectionEnd() == original.selectionEnd(),
          "Failed plain-text search changed the selection");

  QTextCursor after_upper_accent = editor->textCursor();
  after_upper_accent.setPosition(editor->toPlainText().indexOf(u'\u00c9') + 1);
  editor->setTextCursor(after_upper_accent);
  require(!window.find_text(QStringLiteral("\u00c9")),
          "ASCII-only case folding matched a non-ASCII case variant");

  editor->setPlainText(QStringLiteral("x aaa"));
  QTextCursor overlap = editor->textCursor();
  overlap.setPosition(1);
  editor->setTextCursor(overlap);
  require(window.find_text(QStringLiteral("aa")) &&
              editor->textCursor().selectionStart() == 2,
          "Plain search did not find the first overlapping match");
  find_next->trigger();
  require(editor->textCursor().selectionStart() == 3,
          "Find Next skipped an overlapping plain-text match");

  editor->setPlainText(QStringLiteral("x only"));
  QTextCursor before_only = editor->textCursor();
  before_only.setPosition(1);
  editor->setTextCursor(before_only);
  jwpqt::core::JwpSearchOptions wrap;
  wrap.wrap = true;
  require(window.find_text(QStringLiteral("only"), wrap),
          "Plain search did not find its only match");
  const QTextCursor only_match = editor->textCursor();
  require(!window.find_text(QStringLiteral("only"), wrap) &&
              editor->textCursor().selectionStart() ==
                  only_match.selectionStart() &&
              editor->textCursor().selectionEnd() == only_match.selectionEnd(),
          "Wrapped plain search reselected its only current match");
}

void test_jwp_find_uses_legacy_comparison(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"x\uff21y alpha"), paragraph(U""),
                       paragraph(U"beta alpha")};
  source.paragraphs[1].page_break = true;
  const QString path = directory + QStringLiteral("/find.jwp");
  jwpqt::qt::write_jwp_file(path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(path), "Could not open JWP search fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "JWP search window has no editor");

  require(window.find_text(QStringLiteral("A")),
          "JASCII equivalence did not find full-width ASCII");
  require(editor->textCursor().selectedText() == QStringLiteral("\uff21"),
          "JASCII search selected the wrong text");

  jwpqt::core::JwpSearchOptions exact;
  exact.ignore_ascii_case = false;
  exact.jascii_ascii_equivalence = false;
  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(0);
  editor->setTextCursor(cursor);
  require(!window.find_text(QStringLiteral("A"), exact),
          "Exact JWP search matched full-width ASCII");

  require(window.find_text(QStringLiteral("ALPHA")),
          "Case-folded JWP search did not find ASCII text");
  require(editor->textCursor().selectedText() == QStringLiteral("alpha"),
          "JWP search selected the wrong ASCII match");

  QAction* find_next = find_action(window, "findNextAction");
  QAction* find_previous = find_action(window, "findPreviousAction");
  require(find_next != nullptr && find_previous != nullptr,
          "JWP repeat-search actions were not created");
  find_next->trigger();
  require(editor->textCursor().selectedText() == QStringLiteral("alpha") &&
              editor->textCursor().selectionStart() > 10,
          "JWP Find Next did not cross the hard-page-break paragraph");
  find_previous->trigger();
  require(editor->textCursor().selectionStart() < 10,
          "JWP Find Previous did not return to the first match");

  jwpqt::core::JwpSearchOptions wrapped;
  wrapped.direction = jwpqt::core::JwpSearchDirection::kBackward;
  wrapped.wrap = true;
  require(window.find_text(QStringLiteral("ALPHA"), wrapped) &&
              editor->textCursor().selectionStart() > 10,
          "Backward JWP search did not wrap to the final match");

  const QTextCursor selection = editor->textCursor();
  require(!window.find_text(QString::fromUtf8("\xF0\x9F\x98\x80")),
          "Unrepresentable JWP search pattern unexpectedly matched");
  require(editor->textCursor().selectionStart() == selection.selectionStart() &&
              editor->textCursor().selectionEnd() == selection.selectionEnd(),
          "Failed JWP search changed the current selection");
  require(!editor->document()->isModified() &&
              window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "Searching changed the JWP source model");
}

void test_plain_text_replace_actions(const QString& directory) {
  const QString path = directory + QStringLiteral("/replace.txt");
  jwpqt::qt::write_text_file(
      path, jwpqt::core::TextFile{U"aa AA \u00e9 \u00c9",
                                 jwpqt::core::TextEncoding::kUtf8, false});

  PromptingWindow window;
  require(window.open_path(path, jwpqt::core::TextEncoding::kUtf8),
          "Could not open plain replace fixture");
  window.next_replace = jwpqt::qt::ReplaceRequest{
      QStringLiteral("a"), QStringLiteral("x"),
      jwpqt::core::JwpSearchOptions{}, jwpqt::qt::ReplaceMode::kAll};
  QAction* replace = find_action(window, "replaceAction");
  require(replace != nullptr, "Native replace action was not created");
  replace->trigger();

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(window.replace_prompt_count == 1 && editor != nullptr &&
              editor->toPlainText() == QStringLiteral("xx xx \u00e9 \u00c9"),
          "Replace All action did not use ASCII-only comparison");
  editor->undo();
  require(editor->toPlainText() == QStringLiteral("ax xx \u00e9 \u00c9"),
          "Replace All merged independent occurrences into one undo");
  for (int count = 0; count < 3; ++count) {
    editor->undo();
  }
  require(editor->toPlainText() == QStringLiteral("aa AA \u00e9 \u00c9"),
          "Replace All occurrences were not separately undoable");

  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(0);
  editor->setTextCursor(cursor);
  require(window.replace_next(QStringLiteral("a"), QStringLiteral("z")) &&
              editor->toPlainText() == QStringLiteral("az AA \u00e9 \u00c9"),
          "Replace Next did not replace the next candidate");
}

void test_jwp_replace_preserves_structure(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.summary[0] = jwpqt::core::encode_jwp_text(U"replace fixture");
  source.paragraphs = {paragraph(U"\uff21a", 125), paragraph(U"", 150),
                       paragraph(U"A", 175)};
  source.paragraphs[1].page_break = true;
  const QString path = directory + QStringLiteral("/replace.jwp");
  jwpqt::qt::write_jwp_file(path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(path), "Could not open JWP replace fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "JWP replace window has no editor");
  require(window.replace_all(QStringLiteral("a"), QStringLiteral("B")) == 3,
          "JWP Replace All did not use legacy comparison");
  require(editor->toPlainText() == QStringLiteral("BB\n\nB"),
          "JWP Replace All produced the wrong visible text");
  const jwpqt::core::JwpDocument* replaced = window.current_jwp_document();
  require(replaced != nullptr && replaced->summary == source.summary &&
              replaced->paragraphs.size() == 3 &&
              replaced->paragraphs[0].line_spacing == 125 &&
              replaced->paragraphs[1].page_break &&
              replaced->paragraphs[1].line_spacing == 150 &&
              replaced->paragraphs[2].line_spacing == 175,
          "JWP Replace All changed document structure or metadata");

  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(undo != nullptr && redo != nullptr && undo->isEnabled(),
          "Portable JWP undo action was not enabled");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("BB\n\nA"),
          "JWP Replace All merged independent occurrences into one undo");
  undo->trigger();
  undo->trigger();
  require(window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "JWP Replace All occurrences were not separately undoable");
  require(redo->isEnabled(), "Portable JWP redo action was not enabled");
  redo->trigger();
  redo->trigger();
  redo->trigger();
  require(editor->toPlainText() == QStringLiteral("BB\n\nB"),
          "Portable JWP redo did not restore Replace All");
  const int replace_all_caret = editor->textCursor().position();
  editor->insertPlainText(QStringLiteral("C"));
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("BB\n\nB") &&
              editor->textCursor().position() == replace_all_caret,
          "Edit after Replace All restored a stale portable caret");
  undo->trigger();
  undo->trigger();
  undo->trigger();

  QTextCursor selection = editor->textCursor();
  selection.setPosition(2);
  selection.setPosition(3, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  const QString before = editor->toPlainText();
  require(!window.replace_next(QStringLiteral("a"),
                               QString::fromUtf8("\xF0\x9F\x98\x80")) &&
              editor->toPlainText() == before &&
              editor->textCursor().selectionStart() == 2 &&
              editor->textCursor().selectionEnd() == 3 &&
              *window.current_jwp_document() == source,
          "Unrepresentable JWP replacement changed the document");
}

void test_jwp_open_edit_and_save(const QString& directory) {
  const QString source_path = directory + QStringLiteral("/source.jwp");
  const jwpqt::core::JwpDocument source = sample_jwp_document();
  jwpqt::qt::write_jwp_file(source_path, source);
  require(jwpqt::core::has_jwp_document_magic(
              std::string_view(read_bytes(source_path).constData(), 4)),
          "Written JWP file did not contain the JWP magic");

  jwpqt::qt::MainWindow window;
  require(window.open_path_detected(source_path),
          "Detected open did not recognize JWP document");
  require(window.is_jwp_document(), "Window did not retain JWP mode");
  require(window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "JWP open did not retain document metadata");

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "JWP window has no editor");
  require(editor->toPlainText() == QStringLiteral("A\u65e5\u672c\n\u00e9"),
          "JWP body was not exposed as Unicode text");
  require_jwp_layout(editor, source);
  QLabel* label = window.findChild<QLabel*>(QStringLiteral("documentEncoding"));
  require(label != nullptr &&
              label->text() == QStringLiteral("JWP / windows-1252"),
          "JWP status did not show the active code page");

  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(0);
  cursor.setPosition(1, QTextCursor::KeepAnchor);
  cursor.insertText(QStringLiteral("B"));
  require(editor->document()->isModified(),
          "JWP text edit did not mark the document modified");

  jwpqt::core::JwpDocument expected = source;
  expected.paragraphs[0].text =
      jwpqt::core::encode_jwp_text(U"B\u65e5\u672c");
  require(window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == expected,
          "JWP text edit did not preserve non-body metadata and formatting");

  const QString saved_path = directory + QStringLiteral("/saved.jwp");
  require(window.save_path(saved_path), "Could not save edited JWP document");
  require(jwpqt::qt::read_jwp_file(saved_path) == expected,
          "Saved JWP document did not match the edited model");
  require(!editor->document()->isModified(),
          "Saving JWP did not clear modified state");

  const QString plain_path = directory + QStringLiteral("/after-jwp.txt");
  jwpqt::qt::write_text_file(
      plain_path,
      {U"plain", jwpqt::core::TextEncoding::kUtf8, false});
  require(window.open_path(plain_path, jwpqt::core::TextEncoding::kUtf8),
          "Could not switch from JWP to plain text");
  const QTextBlockFormat plain_format =
      editor->document()->begin().blockFormat();
  require(plain_format.leftMargin() == 0.0 &&
              plain_format.rightMargin() == 0.0 &&
              plain_format.textIndent() == 0.0 &&
              !plain_format
                   .property(jwpqt::qt::JwpEditor::kPageBreakProperty)
                   .toBool(),
          "Switching to plain text retained JWP paragraph layout");
}

void test_jwp_code_page_switch(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{{0xc0}}};
  source.paragraphs[0].left_indent = 2;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/code-page.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open JWP code-page fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr &&
              editor->toPlainText() == QStringLiteral("\u00c0"),
          "Default CP1252 interpretation was incorrect");

  QAction* cp1251 =
      find_encoding_action(window, QStringLiteral("windows-1251"));
  require(cp1251 != nullptr, "CP1251 action was not created");
  cp1251->trigger();
  require(window.jwp_code_page() == jwpqt::core::LegacyCodePage::k1251 &&
              editor->toPlainText() == QStringLiteral("\u0410"),
          "Changing JWP code page did not reinterpret extended bytes");
  require_jwp_layout(editor, source);
  require(!editor->document()->isModified(),
          "Changing JWP code page changed the source document");

  const QString saved_path = directory + QStringLiteral("/code-page-saved.jwp");
  require(window.save_path(saved_path), "Could not save code-page fixture");
  require(jwpqt::qt::read_jwp_file(saved_path).paragraphs[0].text ==
              jwpqt::core::JwpText{0xc0},
          "Code-page reinterpretation changed unchanged JWP tokens");
}

void test_jwp_code_page_can_be_selected_before_open(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{{0x80}}};
  const QString source_path = directory + QStringLiteral("/cp1251-only.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  QAction* cp1251 =
      find_encoding_action(window, QStringLiteral("windows-1251"));
  require(cp1251 != nullptr && cp1251->isEnabled(),
          "JWP code page could not be selected before opening a file");
  cp1251->trigger();
  require(window.jwp_code_page() == jwpqt::core::LegacyCodePage::k1251,
          "Preselected JWP code page was not retained");
  require(window.open_path_detected(source_path,
                                    jwpqt::qt::OpenMode::kNonInteractive),
          "JWP file did not use the preselected code page");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr &&
              editor->toPlainText() == QStringLiteral("\u0402"),
          "CP1251-only JWP byte did not decode after preselection");
}

void test_zero_paragraph_jwp_save_is_not_normalized(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.summary[0] = {static_cast<jwpqt::core::JisCode>('Z')};
  const QString source_path = directory + QStringLiteral("/empty-source.jwp");
  const QString saved_path = directory + QStringLiteral("/empty-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open zero-paragraph JWP fixture");
  require(window.save_path(saved_path),
          "Could not save zero-paragraph JWP fixture");
  require(jwpqt::qt::read_jwp_file(saved_path) == source,
          "Unedited zero-paragraph JWP file was normalized on save");

  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Editor was not created for zero-paragraph JWP");
  editor->insertPlainText(QStringLiteral("A"));
  require(editor->document()->isModified(),
          "Editing zero-paragraph JWP did not mark it modified");
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Zero-paragraph edit did not enable portable undo");
  undo->trigger();
  require(editor->toPlainText().isEmpty() &&
              !editor->document()->isModified(),
          "Undo did not restore zero-paragraph JWP saved state");
  const QString undo_saved_path =
      directory + QStringLiteral("/empty-undo-saved.jwp");
  require(window.save_path(undo_saved_path),
          "Could not save undone zero-paragraph JWP fixture");
  require(jwpqt::qt::read_jwp_file(undo_saved_path) == source,
          "Edit then undo normalized a zero-paragraph JWP file");
}

void test_jwp_rejects_lossy_edits(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"A"), paragraph(U""), paragraph(U"B")};
  source.paragraphs[1].page_break = true;
  const QString source_path = directory + QStringLiteral("/page-break.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open page-break fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr && editor->toPlainText() == QStringLiteral("A\n\nB"),
          "Page-break fixture was not rendered predictably");

  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(1);
  cursor.setPosition(2, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  require(editor->toPlainText() == QStringLiteral("A\n\nB") &&
              window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "Edit crossing a hard page break was not reverted");
  require(editor->textCursor().hasSelection() &&
              editor->textCursor().selectionStart() == 1 &&
              editor->textCursor().selectionEnd() == 2,
          "Rejected deletion did not restore the deleted selection");

  cursor = editor->textCursor();
  cursor.setPosition(1);
  cursor.insertText(QStringLiteral("C"));
  require(editor->toPlainText() == QStringLiteral("AC\n\nB"),
          "Representable JWP edit did not apply before rejection test");

  cursor = editor->textCursor();
  cursor.setPosition(0);
  cursor.setPosition(1, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
  cursor.insertText(QString::fromUtf8("\xF0\x9F\x98\x80"));
  require(editor->toPlainText() == QStringLiteral("AC\n\nB"),
          "Unrepresentable JWP text edit was not reverted");
  require(editor->textCursor().hasSelection() &&
              editor->textCursor().selectionStart() == 0 &&
              editor->textCursor().selectionEnd() == 1,
          "Rejected replacement did not restore the prior selection");
  require_jwp_layout(editor, *window.current_jwp_document());
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Valid JWP edit did not enable portable undo");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("A\n\nB") &&
              *window.current_jwp_document() == source &&
              !editor->document()->isModified(),
          "Rejected edit destroyed the prior valid undo entry");
}

void test_jwp_history_actions(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"A")};
  source.paragraphs[0].right_indent = 2;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/history.jwp");
  const QString saved_path = directory + QStringLiteral("/history-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open portable history fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(editor != nullptr && undo != nullptr && redo != nullptr &&
              !undo->isEnabled() && !redo->isEnabled(),
          "Fresh JWP document did not start with empty history");
  QAction* context_undo = nullptr;
  for (QAction* action : editor->actions()) {
    if (action->objectName() == QStringLiteral("undoAction")) {
      context_undo = action;
    }
  }
  require(editor->contextMenuPolicy() == Qt::ActionsContextMenu &&
              context_undo == undo,
          "Editor context menu does not route through portable JWP undo");

  QTextCursor cursor = editor->textCursor();
  cursor.movePosition(QTextCursor::End);
  editor->setTextCursor(cursor);
  editor->insertPlainText(QStringLiteral("B"));
  editor->insertPlainText(QStringLiteral("C"));
  require(editor->toPlainText() == QStringLiteral("ABC") &&
              undo->isEnabled(),
          "JWP typing was not recorded in portable history");
  context_undo->trigger();
  require(editor->toPlainText() == QStringLiteral("A") &&
              !undo->isEnabled() && redo->isEnabled(),
          "Consecutive JWP typing did not coalesce into one undo");
  redo->trigger();
  require(editor->toPlainText() == QStringLiteral("ABC") &&
              undo->isEnabled() && !redo->isEnabled(),
          "Portable JWP redo did not restore typing");
  require_jwp_layout(editor, *window.current_jwp_document());

  require(window.save_path(saved_path), "Could not save history fixture");
  require(undo->isEnabled(), "Saving unexpectedly cleared portable history");
  undo->trigger();
  require(editor->toPlainText() == QStringLiteral("A") &&
              editor->document()->isModified(),
          "Undo after save did not restore pre-edit state");
}

void test_jwp_paragraph_formatting(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"A"), paragraph(U""), paragraph(U"B")};
  source.paragraphs[0].left_indent = 1;
  source.paragraphs[0].right_indent = 2;
  source.paragraphs[0].first_indent = -1;
  source.paragraphs[0].line_spacing = 110;
  source.paragraphs[1].left_indent = 3;
  source.paragraphs[1].right_indent = 4;
  source.paragraphs[1].first_indent = -2;
  source.paragraphs[1].line_spacing = 120;
  source.paragraphs[1].page_break = true;
  source.paragraphs[2].left_indent = 5;
  source.paragraphs[2].right_indent = 6;
  source.paragraphs[2].first_indent = -3;
  source.paragraphs[2].line_spacing = 130;
  const QString source_path = directory + QStringLiteral("/paragraph-format.jwp");
  const QString saved_path =
      directory + QStringLiteral("/paragraph-format-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  PromptingWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open paragraph-format fixture");
  window.show();
  QApplication::processEvents();
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* format = find_action(window, "formatParagraphAction");
  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(editor != nullptr && format != nullptr && undo != nullptr &&
              redo != nullptr && format->isEnabled(),
          "JWP paragraph-format controls were not enabled");

  QTextCursor selection = editor->textCursor();
  selection.setPosition(0);
  selection.setPosition(2, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);

  format->trigger();
  require(window.paragraph_format_prompt_count == 1 &&
              window.offered_paragraph_format ==
                  jwpqt::core::JwpParagraphFormat{3, 4, -2, 120} &&
              *window.current_jwp_document() == source,
          "Cancelled paragraph formatting changed the JWP document");

  const jwpqt::core::JwpParagraphFormat applied{12, 23, -7, 175};
  window.next_paragraph_format = applied;
  format->trigger();
  require(window.paragraph_format_prompt_count == 2,
          "Paragraph-format action did not prompt exactly once");
  const jwpqt::core::JwpDocument formatted = *window.current_jwp_document();
  require(formatted.paragraphs[0].left_indent == 12 &&
              formatted.paragraphs[0].right_indent == 23 &&
              formatted.paragraphs[0].first_indent == -7 &&
              formatted.paragraphs[0].line_spacing == 175 &&
              formatted.paragraphs[1].left_indent == 12 &&
              formatted.paragraphs[1].right_indent == 23 &&
              formatted.paragraphs[1].first_indent == -7 &&
              formatted.paragraphs[1].line_spacing == 175 &&
              formatted.paragraphs[1].page_break &&
              formatted.paragraphs[2] == source.paragraphs[2],
          "Paragraph formatting did not apply to the inclusive selection");
  require(editor->textCursor().selectionStart() == 0 &&
              editor->textCursor().selectionEnd() == 2 &&
              editor->document()->isModified() && undo->isEnabled(),
          "Paragraph formatting did not preserve selection and history");
  require_jwp_layout(editor, formatted);

  require(!window.format_paragraphs({1, 0, -2, 100}) &&
              *window.current_jwp_document() == formatted,
          "Invalid native paragraph format changed the document");
  require(window.format_paragraphs(applied),
          "No-op paragraph formatting was rejected");
  undo->trigger();
  require(*window.current_jwp_document() == source && redo->isEnabled(),
          "Paragraph formatting did not undo as one transaction");
  redo->trigger();
  require(*window.current_jwp_document() == formatted,
          "Paragraph formatting redo did not restore the transaction");
  window.resize(320, 300);
  QApplication::processEvents();
  require(window.format_paragraphs(applied) &&
              *window.current_jwp_document() == formatted,
          "Unchanged paragraph format failed after the page narrowed");
  require(!window.format_paragraphs({255, 255, -127, 100}) &&
              *window.current_jwp_document() == formatted,
          "Paragraph formatting accepted margins wider than the page");

  require(window.save_path(saved_path) &&
              jwpqt::qt::read_jwp_file(saved_path) == formatted,
          "Paragraph formatting did not survive a JWP save");
  const QString text_path = directory + QStringLiteral("/paragraph-format.txt");
  write_bytes(text_path, QByteArray("plain"));
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8) &&
              !format->isEnabled() && !window.format_paragraphs(applied),
          "Plain text did not disable JWP paragraph formatting");

  PromptingWindow mismatched;
  require(mismatched.open_jwp_path(source_path),
          "Could not open paragraph layout failure fixture");
  QTextEdit* mismatched_editor = mismatched.findChild<QTextEdit*>();
  QAction* mismatched_undo = find_action(mismatched, "undoAction");
  require(mismatched_editor != nullptr && mismatched_undo != nullptr,
          "Paragraph layout failure fixture has no controls");
  {
    const QSignalBlocker blocker(mismatched_editor->document());
    mismatched_editor->append(QStringLiteral("orphan block"));
  }
  mismatched_editor->document()->setModified(false);
  require(!mismatched.format_paragraphs(applied) &&
              *mismatched.current_jwp_document() == source &&
              !mismatched_undo->isEnabled() &&
              !mismatched_editor->document()->isModified(),
          "Failed paragraph layout published document or history state");

  PromptingWindow hidden;
  require(hidden.open_jwp_path(source_path) &&
              hidden.format_paragraphs({20, 20, -10, 150}) &&
              hidden.current_jwp_document()->paragraphs[0].left_indent == 20,
          "Hidden editor geometry rejected a valid paragraph format");
}

void test_jwp_page_break_insertion(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"AB"), paragraph(U"CD")};
  source.paragraphs[0].left_indent = 3;
  source.paragraphs[0].right_indent = 4;
  source.paragraphs[0].first_indent = -2;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/page-break.jwp");
  const QString saved_path =
      directory + QStringLiteral("/page-break-saved.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open page-break fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* insert = find_action(window, "insertPageBreakAction");
  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(editor != nullptr && insert != nullptr && undo != nullptr &&
              redo != nullptr && insert->isEnabled() &&
              insert->shortcuts().contains(
                  QKeySequence(Qt::CTRL | Qt::Key_Return)) &&
              insert->shortcuts().contains(
                  QKeySequence(Qt::CTRL | Qt::Key_Enter)),
          "JWP page-break controls were not enabled");

  QTextCursor selection = editor->textCursor();
  selection.setPosition(1);
  selection.setPosition(4, QTextCursor::KeepAnchor);
  editor->setTextCursor(selection);
  insert->trigger();

  const jwpqt::core::JwpDocument inserted =
      *window.current_jwp_document();
  require(inserted.paragraphs.size() == 3 &&
              jwpqt::core::decode_jwp_text(inserted.paragraphs[0].text) ==
                  U"A" &&
              inserted.paragraphs[1].text.empty() &&
              inserted.paragraphs[1].page_break &&
              jwpqt::core::decode_jwp_text(inserted.paragraphs[2].text) ==
                  U"D" &&
              inserted.paragraphs[1].left_indent == 3 &&
              inserted.paragraphs[2].line_spacing == 125,
          "Selected text was not replaced by one formatted page break");
  require(editor->toPlainText() == QStringLiteral("A\n\nD") &&
              !editor->textCursor().hasSelection() &&
              editor->textCursor().position() == 3 &&
              editor->document()->isModified() && undo->isEnabled(),
          "Page-break insertion did not restore the following caret");
  require_jwp_layout(editor, inserted);

  undo->trigger();
  require(*window.current_jwp_document() == source &&
              editor->toPlainText() == QStringLiteral("AB\nCD") &&
              !editor->document()->isModified() && redo->isEnabled(),
          "Page-break insertion did not undo as one transaction");
  redo->trigger();
  require(*window.current_jwp_document() == inserted &&
              editor->textCursor().position() == 3,
          "Page-break insertion redo did not restore structure and caret");
  require(window.save_path(saved_path) &&
              jwpqt::qt::read_jwp_file(saved_path) == inserted,
          "Inserted page break did not survive a JWP save");

  const QString plain_path = directory + QStringLiteral("/page-break.txt");
  write_bytes(plain_path, QByteArray("plain"));
  require(window.open_path(plain_path, jwpqt::core::TextEncoding::kUtf8) &&
              !insert->isEnabled() && !window.insert_page_break(),
          "Plain text did not disable JWP page-break insertion");
}

void test_jwp_rejects_non_bmp_edit(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"AB")};
  const QString source_path = directory + QStringLiteral("/non-bmp.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open non-BMP rejection fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "Editor was not created for non-BMP fixture");
  QTextCursor cursor = editor->textCursor();
  cursor.setPosition(1);
  cursor.insertText(QString::fromUtf8("\xF0\x9F\x98\x80"));
  require(editor->toPlainText() == QStringLiteral("AB") &&
              window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source &&
              !editor->document()->isModified(),
          "Rejected non-BMP edit corrupted the JWP document");
}

void test_jwp_wnn_conversion(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x2422};
  source.paragraphs[0].first_indent = 1;
  source.paragraphs[0].line_spacing = 125;
  const QString source_path = directory + QStringLiteral("/convert.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path),
          "Could not load native WNN resources");
  require(!window.load_wnn_resources(
              directory + QStringLiteral("/missing.dix"), fixture.data_path,
              fixture.preferences_path,
              jwpqt::qt::OpenMode::kNonInteractive),
          "Missing replacement WNN resources unexpectedly loaded");
  require(window.open_jwp_path(source_path),
          "Could not open native WNN conversion fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* convert = find_action(window, "convertSelectionAction");
  QAction* next = find_action(window, "nextCandidateAction");
  QAction* accept = find_action(window, "acceptCandidateAction");
  QAction* page_break = find_action(window, "insertPageBreakAction");
  require(editor != nullptr && convert != nullptr && next != nullptr &&
              accept != nullptr && page_break != nullptr,
          "Native WNN conversion actions were not created");

  editor->selectAll();
  require(convert->isEnabled(),
          "Native WNN conversion was not enabled for selected kana");
  convert->trigger();
  require(window.conversion_active() && editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3021},
          "Native WNN conversion did not display the preferred candidate");
  require(!page_break->isEnabled() && !window.insert_page_break() &&
              window.conversion_active() && editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3021},
          "Page-break insertion accepted an active WNN conversion");
  window.show();
  editor->setFocus();
  QApplication::processEvents();
  QKeyEvent next_key(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
  QApplication::sendEvent(editor, &next_key);
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText{0x3022},
          "Space did not cycle native WNN candidates");
  QKeyEvent accept_key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(editor, &accept_key);
  require(!window.conversion_active() && !editor->isReadOnly() &&
              QFile::exists(fixture.preferences_path),
          "Escape did not accept and persist native WNN conversion");

  QAction* undo = find_action(window, "undoAction");
  QAction* redo = find_action(window, "redoAction");
  require(undo != nullptr && redo != nullptr && undo->isEnabled(),
          "Native conversion did not create a portable undo entry");
  undo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText{0x2422},
          "Undo did not restore the original conversion input");
  redo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText{0x3022},
          "Redo did not restore the accepted conversion candidate");
  require_jwp_layout(editor, *window.current_jwp_document());

  jwpqt::qt::MainWindow reopened;
  require(reopened.load_wnn_resources(fixture.index_path, fixture.data_path,
                                      fixture.preferences_path) &&
              reopened.open_jwp_path(source_path),
          "Could not reopen persisted WNN preference fixture");
  QTextEdit* reopened_editor = reopened.findChild<QTextEdit*>();
  require(reopened_editor != nullptr, "Reopened WNN window has no editor");
  QTextCursor reversed = reopened_editor->textCursor();
  reversed.setPosition(1);
  reversed.setPosition(0, QTextCursor::KeepAnchor);
  reopened_editor->setTextCursor(reversed);
  require(reopened.convert_selection() &&
              reopened.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3022},
          "Native WNN conversion did not restore the learned candidate");
  require(reopened_editor->textCursor().position() ==
              reopened_editor->textCursor().selectionStart(),
          "Native conversion did not preserve reversed-caret orientation");
  require(reopened.accept_conversion(),
          "Could not accept the restored WNN candidate");
}

void test_jwp_wnn_conversion_boundaries(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{},
                       jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x2422};
  source.paragraphs[1].text = {0x2422};
  const QString source_path = directory + QStringLiteral("/convert-range.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path) &&
              window.open_jwp_path(source_path),
          "Could not prepare WNN range fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* convert = find_action(window, "convertSelectionAction");
  require(editor != nullptr && convert != nullptr,
          "WNN range fixture has no conversion controls");
  editor->selectAll();
  require(!convert->isEnabled() && !window.convert_selection() &&
              *window.current_jwp_document() == source,
          "Cross-paragraph WNN selection was not rejected");
}

void test_jwp_wnn_preference_write_failure(const QString& directory) {
  const WnnFixture fixture = write_wnn_fixture(directory);
  const QString blocked_path = directory + QStringLiteral("/blocked-user.sel");
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  source.paragraphs[0].text = {0x2422};
  const QString source_path = directory + QStringLiteral("/blocked-save.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    blocked_path) &&
              window.open_jwp_path(source_path),
          "Could not prepare WNN preference failure fixture");
  require(QDir().mkpath(blocked_path),
          "Could not block the WNN preference output path");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, "WNN preference failure window has no editor");
  editor->selectAll();
  require(window.convert_selection() && window.cycle_conversion() &&
              window.accept_conversion(),
          "Preference write failure prevented candidate acceptance");
  require(!window.conversion_active() && !editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3022},
          "Preference write failure rolled back accepted document state");
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Preference write failure discarded conversion history");
}

void send_text_key(QTextEdit* editor, int key, const QString& text,
                   Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
  QApplication::sendEvent(editor, &event);
}

void test_jwp_kana_input_mode(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  const QString source_path = directory + QStringLiteral("/kana-input.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open kana-input fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* kana = find_action(window, "kanaInputAction");
  QLabel* input_mode = window.findChild<QLabel*>(QStringLiteral("inputMode"));
  require(editor != nullptr && kana != nullptr && input_mode != nullptr &&
              kana->isEnabled() && !window.kana_input_enabled() &&
              input_mode->text() == QStringLiteral("Direct"),
          "Kana-input controls did not start in direct mode");

  kana->trigger();
  require(window.kana_input_enabled() && kana->isChecked() &&
              input_mode->text() == QStringLiteral("Kana"),
          "Kana-input action did not enable composition");
  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("\u304b") &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x242b},
          "Romaji input did not insert synchronized hiragana");

  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_Backspace, QString());
  require(editor->toPlainText() == QStringLiteral("\u304b"),
          "Backspace mutated the document instead of discarding pending kana");
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText() == QStringLiteral("\u304b\u3042"),
          "Discarded composition leaked into the next kana input");

  send_text_key(editor, Qt::Key_K, QStringLiteral("K"), Qt::ShiftModifier);
  send_text_key(editor, Qt::Key_A, QStringLiteral("A"), Qt::ShiftModifier);
  require(editor->toPlainText() == QStringLiteral("\u304b\u3042\u30ab"),
          "Uppercase romaji did not insert katakana");

  kana->trigger();
  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(!window.kana_input_enabled() &&
              editor->toPlainText() == QStringLiteral("\u304b\u3042\u30abka"),
          "Direct mode unexpectedly composed romaji");

  kana->trigger();
  send_text_key(editor, Qt::Key_N, QStringLiteral("n"));
  const QString saved_path = directory + QStringLiteral("/kana-pending.jwp");
  require(window.save_path(saved_path),
          "Could not save pending kana fixture");
  const std::u32string saved_text = jwpqt::core::decode_jwp_text(
      jwpqt::qt::read_jwp_file(saved_path).paragraphs[0].text);
  require(editor->toPlainText().endsWith(QStringLiteral("\u3093")) &&
              !saved_text.empty() && saved_text.back() == U'\u3093',
          "Save did not commit pending kana before writing");

  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  QAction* undo = find_action(window, "undoAction");
  require(undo != nullptr && undo->isEnabled(),
          "Kana input did not create portable history");
  undo->trigger();
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(editor->toPlainText().endsWith(QStringLiteral("\u3042")),
          "Undo action left pre-command kana composition pending");

  const QString text_path = directory + QStringLiteral("/plain-kana.txt");
  write_bytes(text_path, QByteArray("plain"));
  require(window.open_path(text_path, jwpqt::core::TextEncoding::kUtf8) &&
              !kana->isEnabled() && !kana->isChecked() &&
              !window.kana_input_enabled(),
          "Plain text document did not disable JWP kana input");
}

void test_jwp_automatic_wnn_conversion(const QString& directory) {
  const WnnFixture fixture = write_automatic_wnn_fixture(directory);
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{}};
  const QString source_path = directory + QStringLiteral("/automatic.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.load_wnn_resources(fixture.index_path, fixture.data_path,
                                    fixture.preferences_path) &&
              window.open_jwp_path(source_path),
          "Could not prepare automatic WNN fixture");
  QTextEdit* editor = window.findChild<QTextEdit*>();
  QAction* kana = find_action(window, "kanaInputAction");
  QAction* undo = find_action(window, "undoAction");
  require(editor != nullptr && kana != nullptr && undo != nullptr,
          "Automatic WNN controls were not created");
  kana->trigger();
  window.show();
  editor->setFocus();
  QApplication::processEvents();

  send_text_key(editor, Qt::Key_K, QStringLiteral("K"), Qt::ShiftModifier);
  send_text_key(editor, Qt::Key_A, QStringLiteral("a"));
  require(!window.conversion_active() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x242b} &&
              editor->extraSelections().size() == 1,
          "Extendable automatic WNN key did not remain pending");

  send_text_key(editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(editor, Qt::Key_I, QStringLiteral("i"));
  require(window.conversion_active() && editor->isReadOnly() &&
              window.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText{0x3023},
          "Terminal automatic WNN key did not display its candidate");
  send_text_key(editor, Qt::Key_Escape, QString());
  require(!window.conversion_active() && !editor->isReadOnly() &&
              editor->extraSelections().empty(),
          "Escape did not accept automatic WNN conversion");
  undo->trigger();
  require(window.current_jwp_document()->paragraphs[0].text ==
              jwpqt::core::JwpText({0x242b, 0x242d}),
          "Automatic conversion undo did not restore composed kana");

  jwpqt::qt::MainWindow backed_off;
  require(backed_off.load_wnn_resources(
              fixture.index_path, fixture.data_path,
              directory + QStringLiteral("/automatic-backoff.sel")) &&
              backed_off.open_jwp_path(source_path),
          "Could not prepare automatic WNN backoff fixture");
  QTextEdit* backoff_editor =
      backed_off.findChild<QTextEdit*>();
  QAction* backoff_kana = find_action(backed_off, "kanaInputAction");
  QAction* backoff_undo = find_action(backed_off, "undoAction");
  require(backoff_editor != nullptr && backoff_kana != nullptr &&
              backoff_undo != nullptr,
          "Automatic WNN backoff controls were not created");
  backoff_kana->trigger();
  backed_off.show();
  backoff_editor->setFocus();
  QApplication::processEvents();
  send_text_key(backoff_editor, Qt::Key_K, QStringLiteral("K"),
                Qt::ShiftModifier);
  send_text_key(backoff_editor, Qt::Key_A, QStringLiteral("a"));
  send_text_key(backoff_editor, Qt::Key_K, QStringLiteral("k"));
  send_text_key(backoff_editor, Qt::Key_U, QStringLiteral("u"));
  require(backed_off.conversion_active() && backoff_editor->isReadOnly() &&
              !backoff_editor->textCursor().hasSelection() &&
              backoff_editor->textCursor().position() == 2 &&
              backoff_editor->extraSelections().size() == 1 &&
              backed_off.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x3021, 0x242f}),
          "Automatic WNN backoff did not preserve its suffix and caret");
  send_text_key(backoff_editor, Qt::Key_Space, QStringLiteral(" "));
  require(backed_off.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x3022, 0x242f}) &&
              backoff_editor->textCursor().position() == 2,
          "Automatic WNN cycling did not preserve the unmatched suffix");
  send_text_key(backoff_editor, Qt::Key_Escape, QString());
  backoff_undo->trigger();
  require(backed_off.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x242b, 0x242f}),
          "Automatic WNN backoff undo did not restore the full kana input");

  jwpqt::qt::MainWindow externally_edited;
  require(externally_edited.open_jwp_path(
              source_path, jwpqt::core::LegacyCodePage::k1252,
              jwpqt::qt::OpenMode::kNonInteractive) &&
              externally_edited.load_wnn_resources(
                  fixture.index_path, fixture.data_path,
                  fixture.preferences_path,
                  jwpqt::qt::OpenMode::kNonInteractive),
          "Could not prepare external-edit automatic WNN test");
  QTextEdit* external_editor =
      externally_edited.findChild<QTextEdit*>();
  QAction* external_kana =
      externally_edited.findChild<QAction*>(QStringLiteral("kanaInputAction"));
  require(external_editor != nullptr && external_kana != nullptr,
          "External-edit automatic WNN controls were not created");
  externally_edited.show();
  external_editor->setFocus();
  QCoreApplication::processEvents();
  external_kana->trigger();
  send_text_key(external_editor, Qt::Key_K, QStringLiteral("K"));
  send_text_key(external_editor, Qt::Key_A, QStringLiteral("a"));
  require(external_editor->extraSelections().size() == 1,
          "External-edit test did not create a waiting WNN range");

  external_editor->insertPlainText(QStringLiteral("x"));
  QCoreApplication::processEvents();
  require(external_editor->extraSelections().empty() &&
              !externally_edited.conversion_active() &&
              externally_edited.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x242b, 0x0078}),
          "External document edit did not invalidate waiting WNN conversion");
  require(externally_edited.save_path(directory + QStringLiteral("/external.jwp")) &&
              externally_edited.current_jwp_document()->paragraphs[0].text ==
                  jwpqt::core::JwpText({0x242b, 0x0078}),
          "Saving after an external edit converted a stale WNN range");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  try {
    QTemporaryDir directory(QDir::tempPath() +
                            QStringLiteral("/jwpqt-window-test-XXXXXX"));
    require(directory.isValid(), "Could not create temporary test directory");
    test_explicit_open_and_encoding_action(directory.path());
    test_leaving_utf8_drops_bom(directory.path());
    test_detected_open(directory.path());
    test_detected_bom_is_preserved(directory.path());
    test_detection_prompt_and_cancellation(directory.path());
    test_ascii_and_unknown_prompts(directory.path());
    test_plain_text_find_actions(directory.path());
    test_jwp_find_uses_legacy_comparison(directory.path());
    test_plain_text_replace_actions(directory.path());
    test_jwp_replace_preserves_structure(directory.path());
    test_jwp_open_edit_and_save(directory.path());
    test_jwp_code_page_switch(directory.path());
    test_jwp_code_page_can_be_selected_before_open(directory.path());
    test_zero_paragraph_jwp_save_is_not_normalized(directory.path());
    test_jwp_rejects_lossy_edits(directory.path());
    test_jwp_rejects_non_bmp_edit(directory.path());
    test_jwp_history_actions(directory.path());
    test_jwp_paragraph_formatting(directory.path());
    test_jwp_page_break_insertion(directory.path());
    test_jwp_wnn_conversion(directory.path());
    test_jwp_wnn_conversion_boundaries(directory.path());
    test_jwp_wnn_preference_write_failure(directory.path());
    test_jwp_kana_input_mode(directory.path());
    test_jwp_automatic_wnn_conversion(directory.path());
    std::cout << "All main window tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
