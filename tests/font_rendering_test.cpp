// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>

#include "application_settings.h"
#include "application_settings_dialog.h"
#include "japanese_fonts.h"
#include "jwp_editor.h"
#include "kanji_info_dialog.h"
#include "main_window.h"

namespace qt = jwpqt::qt;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void test_fallback_and_big() {
  qt::ApplicationSettings settings;
  settings.fonts[0] = {QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), 19, false};
  settings.fonts[1] = {{}, 23, false};
  settings.fonts[2] = {QStringLiteral("missing-jwpqt-font-123"), 90, false};
  QWidget owner;
  auto* list = new QLabel(&owner);
  qt::assign_japanese_font(*list, qt::JapaneseFontRole::kList);
  const auto warnings = qt::set_japanese_fonts(owner, settings);
  require(list->font().pixelSize() == 23 && !warnings.empty() &&
          settings.fonts[2].size == 90, "Missing List font did not inherit Edit size without rewriting settings");
  settings.fonts[1].family = QStringLiteral("missing-jwpqt-edit-456");
  qt::set_japanese_fonts(owner, settings);
  require(list->font().pixelSize() == 19 && list->font().families().contains(settings.fonts[0].family),
          "Nested missing font fallback lost System family or size");
  settings.fonts[4] = {{}, 31, false};
  qt::set_japanese_fonts(owner, settings);
  require(qt::japanese_font(owner, qt::JapaneseFontRole::kBitmap).pixelSize() == 31,
          "Automatic Bitmap font did not inherit File");
  auto* information = new qt::KanjiInfoDialog(nullptr, {}, &owner);
  require(information->set_character(U'\u611b'), "Big character fixture failed");
  auto* character = information->findChild<QLabel*>(QStringLiteral("kanjiInfoCharacter"));
  information->show(); QApplication::processEvents();
  const auto check = [&] {
    const auto bounds = QFontMetricsF(character->font()).boundingRect(character->text());
    require(bounds.width() <= character->contentsRect().width() &&
            bounds.height() <= character->contentsRect().height(), "Big glyph is clipped");
  };
  check(); const int small = character->font().pixelSize();
  information->resize(1250, 900); QApplication::processEvents(); check();
  require(character->font().pixelSize() > small, "Big glyph did not grow with its pane");
  information->resize(800, 420); QApplication::processEvents(); check();
  for (char32_t value : {U'W', U'\u0394', U'\U0001f600'}) {
    require(information->set_character(value), "Big Unicode character rejected"); check();
  }
  require(information->set_character(U'\u611b'), "Big character preview failed"); check();
  information->grab().save(QStringLiteral("font-big-fit.png"));
}

void test_bitmap() {
  qt::ApplicationSettings settings;
  constexpr auto bitmap = static_cast<std::size_t>(qt::JapaneseFontRole::kBitmap);
  settings.fonts[bitmap] = {{}, 24, false};
  auto stored = qt::read_application_settings("clip_font.size=24\nclip_font.automatic=false\nno_BITMAP=false\nFuture=opaque\n");
  require(stored.fonts[bitmap].size == 24 && !stored.fonts[bitmap].automatic &&
          !stored.omit_clipboard_bitmap && qt::read_application_settings(qt::write_application_settings(stored)).fonts[bitmap].size == 24,
          "Bitmap settings aliases or roundtrip failed");
  bool invalid = false;
  try { (void)qt::read_application_settings("no_BITMAP=2\nClipboard_Omit_Bitmap=false\n"); }
  catch (const std::exception&) { invalid = true; }
  require(invalid, "Invalid overridden bitmap preference accepted");
  QWidget owner;
  auto* editor = new qt::JwpEditor(&owner);
  editor->resize(400, 200);
  qt::assign_japanese_font(*editor, qt::JapaneseFontRole::kFile);
  qt::set_japanese_fonts(owner, settings);
  const QString text = QString(QChar(0xfeff)) + QStringLiteral("\u611b\u00a0ABC\n\U0001f600");
  editor->insertPlainText(text); editor->selectAll();
  const int undo = editor->document()->availableUndoSteps();
  const int anchor = editor->textCursor().anchor(), position = editor->textCursor().position();
  editor->copy();
  auto* clipboard = QApplication::clipboard();
  require(clipboard->text() == text && clipboard->mimeData()->hasHtml() && clipboard->mimeData()->hasImage(),
          "Bitmap copy lost literal plain text or rich MIME");
  auto image = qvariant_cast<QImage>(clipboard->mimeData()->imageData());
  require(!image.isNull() && image.width() <= 8192 && image.height() <= 8192 && image.pixelColor(0, 0) == Qt::white,
          "Bitmap dimensions or paper incorrect");
  bool ink = false;
  for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x)
    ink |= image.pixelColor(x, y) != Qt::white;
  require(ink, "Bitmap has no rendered text");
  const int height = image.height(); image.save(QStringLiteral("clipboard-bitmap.png"));
  settings.fonts[bitmap].size = 48;
  qt::set_japanese_fonts(owner, settings); editor->copy();
  require(qvariant_cast<QImage>(clipboard->mimeData()->imageData()).height() > height,
          "Bitmap font size did not affect image");
  settings.omit_clipboard_bitmap = true;
  qt::set_japanese_fonts(owner, settings); editor->copy();
  require(!clipboard->mimeData()->hasImage() && clipboard->text() == text && clipboard->mimeData()->hasHtml(),
          "Omit bitmap removed other formats or retained image");
  require(editor->document()->availableUndoSteps() == undo && editor->textCursor().anchor() == anchor &&
          editor->textCursor().position() == position && qt::document_plain_text(*editor->document()) == text,
          "Font application or Copy changed document/history/selection");
  editor->undo(); require(editor->document()->isEmpty(), "Copy damaged undo");
  settings.omit_clipboard_bitmap = false;
  qt::set_japanese_fonts(owner, settings);
  editor->setPlainText(QString(262145, QLatin1Char('a'))); editor->selectAll(); editor->copy();
  require(!clipboard->mimeData()->hasImage() && clipboard->text().size() == 262145,
          "Oversized bitmap copy truncated or discarded text");
  settings.fonts[bitmap].size = 1024;
  qt::set_japanese_fonts(owner, settings);
  editor->setPlainText(QStringLiteral("\u611b")); editor->selectAll(); editor->copy();
  require(clipboard->mimeData()->hasImage(), "Bounded large-font selection lost its bitmap");
  const QString too_large(100, QChar(0x611b));
  editor->setPlainText(too_large); editor->selectAll(); editor->copy();
  require(!clipboard->mimeData()->hasImage() && clipboard->text() == too_large,
          "Pixel bounds must retain all text without allocating an oversized image");
  settings.fonts[bitmap].size = 48;
  qt::set_japanese_fonts(owner, settings);
  QApplication::processEvents();

  qt::ApplicationSettingsDialog options(settings);
  auto* omit = options.findChild<QCheckBox*>(QStringLiteral("settingsOmitClipboardBitmap"));
  require(omit && !omit->isChecked(), "Bitmap omission control missing");
  omit->setChecked(true);
  qt::ApplicationSettingsDialog cancelled(settings);
  cancelled.findChild<QCheckBox*>(QStringLiteral("settingsOmitClipboardBitmap"))->setChecked(true);
  cancelled.reject();
  require(!cancelled.settings().omit_clipboard_bitmap, "Cancelled bitmap options changed settings");
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(options.settings().omit_clipboard_bitmap, "Bitmap omission control not applied");

  QTemporaryDir directory;
  require(directory.isValid(), "Font settings fixture directory failed");
  qt::MainWindow native;
  const auto path = directory.filePath(QStringLiteral("font-settings.cfg"));
  require(native.load_application_settings(path) && native.apply_application_settings(settings) &&
          native.save_application_settings(), "Could not persist Bitmap preferences");
  qt::MainWindow restarted;
  require(restarted.load_application_settings(path) && restarted.application_settings().fonts[bitmap].size == 48,
          "Restart lost Bitmap preferences");
  native.active_editor()->insertPlainText(QStringLiteral("\u611b ABC"));
  const auto before = *native.current_jwp_document();
  native.active_editor()->selectAll();
  native.active_editor()->copy();
  require(clipboard->mimeData()->hasImage() && *native.current_jwp_document() == before,
          "Native Copy changed the model or omitted its bitmap");
  native.findChild<QAction*>(QStringLiteral("undoAction"))->trigger();
  require(qt::document_plain_text(*native.active_editor()->document()).isEmpty(), "Native bitmap Copy damaged undo");
}

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try { test_fallback_and_big(); test_bitmap(); std::cout << "Font rendering tests passed\n"; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
