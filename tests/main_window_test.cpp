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
  std::vector<jwpqt::core::TextEncoding> offered_encodings;
  QString explanation;

 protected:
  std::optional<jwpqt::core::TextEncoding> prompt_for_encoding(
      const std::vector<jwpqt::core::TextEncoding>& candidates,
      const QString& prompt) override {
    offered_encodings = candidates;
    explanation = prompt;
    return next_encoding;
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

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  try {
    QTemporaryDir directory(QDir::currentPath() +
                            QStringLiteral("/jwpqt-window-test-XXXXXX"));
    require(directory.isValid(), "Could not create temporary test directory");
    test_explicit_open_and_encoding_action(directory.path());
    test_leaving_utf8_drops_bom(directory.path());
    test_detected_open(directory.path());
    test_detected_bom_is_preserved(directory.path());
    test_detection_prompt_and_cancellation(directory.path());
    test_ascii_and_unknown_prompts(directory.path());
    std::cout << "All main window tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
