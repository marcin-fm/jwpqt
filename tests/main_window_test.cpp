// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextDocument>

#include "file_io.h"
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

class PromptingWindow : public jwpqt::qt::MainWindow {
 public:
  std::optional<jwpqt::core::TextEncoding> next_encoding;
  std::optional<jwpqt::qt::SearchRequest> next_search;
  std::optional<jwpqt::qt::ReplaceRequest> next_replace;
  std::vector<jwpqt::core::TextEncoding> offered_encodings;
  QString explanation;
  int search_prompt_count = 0;
  int replace_prompt_count = 0;

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

  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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

  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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

  editor->undo();
  require(editor->toPlainText() == QStringLiteral("\uff21B\n\nB"),
          "JWP Replace All merged independent occurrences into one undo");
  editor->undo();
  editor->undo();
  require(window.current_jwp_document() != nullptr &&
              *window.current_jwp_document() == source,
          "JWP Replace All occurrences were not separately undoable");

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

  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
  require(editor != nullptr, "JWP window has no editor");
  require(editor->toPlainText() == QStringLiteral("A\u65e5\u672c\n\u00e9"),
          "JWP body was not exposed as Unicode text");
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
}

void test_jwp_code_page_switch(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {jwpqt::core::JwpParagraph{{0xc0}}};
  const QString source_path = directory + QStringLiteral("/code-page.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open JWP code-page fixture");
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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

  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
  require(editor != nullptr, "Editor was not created for zero-paragraph JWP");
  editor->insertPlainText(QStringLiteral("A"));
  require(editor->document()->isModified(),
          "Editing zero-paragraph JWP did not mark it modified");
  editor->undo();
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
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
  editor->undo();
  require(editor->toPlainText() == QStringLiteral("A\n\nB") &&
              *window.current_jwp_document() == source &&
              !editor->document()->isModified(),
          "Rejected edit destroyed the prior valid undo entry");
}

void test_jwp_rejects_non_bmp_edit(const QString& directory) {
  jwpqt::core::JwpDocument source;
  source.paragraphs = {paragraph(U"AB")};
  const QString source_path = directory + QStringLiteral("/non-bmp.jwp");
  jwpqt::qt::write_jwp_file(source_path, source);

  jwpqt::qt::MainWindow window;
  require(window.open_jwp_path(source_path),
          "Could not open non-BMP rejection fixture");
  QPlainTextEdit* editor = window.findChild<QPlainTextEdit*>();
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
    std::cout << "All main window tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
