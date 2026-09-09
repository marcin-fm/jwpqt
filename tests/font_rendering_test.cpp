// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <iostream>
#include <stdexcept>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
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
#include "text_bridge.h"
#include "jwpqt/core/jwp_text_codec.h"

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
  settings.vertical_clipboard_bitmap = true;
  settings.color_clipboard_bitmap = true;
  const auto path = directory.filePath(QStringLiteral("font-settings.cfg"));
  require(native.load_application_settings(path) && native.apply_application_settings(settings) &&
          native.save_application_settings(), "Could not persist Bitmap preferences");
  qt::MainWindow restarted;
  require(restarted.load_application_settings(path) && restarted.application_settings().fonts[bitmap].size == 48 &&
          restarted.application_settings().vertical_clipboard_bitmap && restarted.application_settings().color_clipboard_bitmap,
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

void test_vertical_and_color_bitmap() {
  namespace core = jwpqt::core;
  auto settings = qt::read_application_settings(
      "Bitmap.Vert=true\nColorKanji_Clipboard=true\nFuture=opaque\n");
  require(settings.vertical_clipboard_bitmap && settings.color_clipboard_bitmap,
          "Clipboard rendering settings did not parse");
  const auto serialized = qt::write_application_settings(settings);
  require(qt::read_application_settings(serialized).vertical_clipboard_bitmap &&
          qt::read_application_settings(serialized).color_clipboard_bitmap && serialized.find("Future=opaque") != std::string::npos,
          "Clipboard rendering settings did not roundtrip");
  for (const char* bad : {"clip_font.vertical=2\nBitmap.Vert=true\n", "colorkanji_bitmap=2\nColorKanji_Clipboard=true\n"}) {
    bool rejected = false;
    try { (void)qt::read_application_settings(bad); } catch (const std::exception&) { rejected = true; }
    require(rejected, "Invalid earlier clipboard setting was ignored");
  }
  constexpr auto bitmap_role = static_cast<std::size_t>(qt::JapaneseFontRole::kBitmap);
  settings.fonts[4] = {{}, 23, false};
  settings.fonts[bitmap_role] = {QStringLiteral("Noto Sans CJK JP"), 36, true};
  QWidget owner;
  auto* editor = new qt::JwpEditor(&owner); editor->resize(600, 250);
  qt::assign_japanese_font(*editor, qt::JapaneseFontRole::kFile);
  qt::set_japanese_fonts(owner, settings);
  require(qt::japanese_font(*editor, qt::JapaneseFontRole::kBitmap).pixelSize() == 36,
          "Vertical bitmap did not use its own font despite Automatic");
  settings.vertical_clipboard_bitmap = false; qt::set_japanese_fonts(owner, settings);
  require(qt::japanese_font(*editor, qt::JapaneseFontRole::kBitmap).pixelSize() == 23,
          "Horizontal automatic bitmap stopped inheriting File");
  settings.fonts[bitmap_role] = {QStringLiteral("Noto Sans CJK JP"), 36, false};
  const auto image = [&](const QString& text, bool vertical) {
    settings.vertical_clipboard_bitmap = vertical; qt::set_japanese_fonts(owner, settings);
    editor->setPlainText(text); editor->clear_kanji_colors(); editor->selectAll(); editor->copy();
    require(QApplication::clipboard()->text() == text && QApplication::clipboard()->mimeData()->hasHtml(),
            "Vertical copy changed text or rich formats");
    return qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData());
  };
  const auto latin = image(QStringLiteral("ABC"), false);
  require(!latin.isNull() && image(QStringLiteral("ABC"), true) == latin, "Vertical bitmap rotated or changed Latin");
  const auto punctuation = image(QStringLiteral("\u30fc"), false);
  require(!punctuation.isNull() && image(QStringLiteral("\u30fc"), true) != punctuation,
          "TrueType vertical punctuation incorrectly used the raster exception");
  const auto horizontal = image(QStringLiteral("\u611b"), false);
  const auto vertical = image(QStringLiteral("\u611b"), true);
  require(!vertical.isNull() && vertical.size() == horizontal.size() && vertical != horizontal,
          "Japanese bitmap rotation missing or entire image rotated");
  const QString text = QStringLiteral("xx\u611bA") + qt::to_qstring(core::decode_jwp_text({0x5021}));
  editor->setPlainText(text);
  core::JwpDocument model; model.paragraphs.emplace_back(); model.paragraphs.back().text = core::encode_jwp_text(qt::from_qstring(text));
  core::KanjiColorList list; list.add(0x3026);
  core::KanjiColorPolicy policy;
  policy.list_mode = core::KanjiListColorMode::kMatch;
  policy.list_color = {220, 0, 0}; policy.colorize_uncommon = true; policy.uncommon_color = {0, 180, 0};
  editor->apply_kanji_colors(model, list, policy);
  QTextCursor selected(editor->document()); selected.setPosition(2); selected.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  editor->setTextCursor(selected);
  QTextEdit::ExtraSelection transient; transient.cursor = selected; transient.format.setForeground(Qt::blue);
  transient.format.setBackground(Qt::yellow); editor->set_transient_extra_selections({transient});
  const int undo = editor->document()->availableUndoSteps();
  const auto color_counts = [](const QImage& bitmap) {
    int red = 0, green = 0, blue = 0;
    for (int y = 0; y < bitmap.height(); ++y) for (int x = 0; x < bitmap.width(); ++x) {
      const auto color = bitmap.pixelColor(x, y);
      red += color.red() > color.green() + 20 && color.red() > color.blue() + 20;
      green += color.green() > color.red() + 20 && color.green() > color.blue() + 20;
      blue += color.blue() > color.red() + 20 && color.blue() > color.green() + 20;
    }
    return std::array<int, 3>{red, green, blue};
  };
  for (bool orientation : {false, true}) {
    settings.vertical_clipboard_bitmap = orientation; settings.color_clipboard_bitmap = true;
    qt::set_japanese_fonts(owner, settings); editor->copy();
    const auto colored = qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData());
    const auto counts = color_counts(colored);
    require(counts[0] > 0 && counts[1] > 0 && counts[2] == 0, "Bitmap lost persistent colors or copied transient highlights");
    require(QApplication::clipboard()->text() == text.mid(2), "Colored partial copy changed selected text");
    if (orientation) colored.save(QStringLiteral("clipboard-vertical-color.png"));
    settings.color_clipboard_bitmap = false; qt::set_japanese_fonts(owner, settings); editor->copy();
    require(color_counts(qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData())) == std::array<int, 3>{0, 0, 0},
            "Disabled bitmap color policy retained colors");
  }
  settings.color_clipboard_bitmap = true; qt::set_japanese_fonts(owner, settings);
  policy.list_mode = core::KanjiListColorMode::kNoMatch; editor->apply_kanji_colors(model, list, policy); editor->copy();
  const auto nonmembers = color_counts(qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData()));
  require(nonmembers[0] > 0 && nonmembers[1] == 0 && nonmembers[2] == 0,
          "Non-member bitmap color did not take precedence over uncommon coloring");
  policy.list_mode = core::KanjiListColorMode::kOff; editor->apply_kanji_colors(model, list, policy); editor->copy();
  require(color_counts(qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData())) == std::array<int, 3>{0, 0, 0},
          "Inactive list mode allowed uncommon bitmap colors");
  require(editor->document()->availableUndoSteps() == undo && editor->textCursor().selectionStart() == 2 &&
          qt::document_plain_text(*editor->document()) == text, "Bitmap rendering changed source selection/history");
  settings.fonts[bitmap_role].automatic = true;
  qt::ApplicationSettingsDialog options(settings);
  auto* automatic = options.findChild<QCheckBox*>(QStringLiteral("settingsFontAuto7"));
  auto* family = options.findChild<QComboBox*>(QStringLiteral("settingsFont7"));
  require(automatic && automatic->isChecked() && !automatic->isEnabled() && family && family->isEnabled(),
          "Vertical bitmap font controls did not preserve and override Automatic");
  auto* direction = options.findChild<QCheckBox*>(QStringLiteral("settingsVerticalClipboardBitmap"));
  auto* colors = options.findChild<QCheckBox*>(QStringLiteral("settingsColorClipboardBitmap"));
  require(direction && colors && direction->isChecked() && colors->isChecked(), "Clipboard rendering controls missing");
  direction->setChecked(false); colors->setChecked(false);
  require(automatic->isEnabled() && !family->isEnabled(), "Horizontal bitmap controls did not restore inheritance");
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(!options.settings().vertical_clipboard_bitmap && !options.settings().color_clipboard_bitmap, "Clipboard controls did not apply");
  qt::ApplicationSettingsDialog cancelled(settings);
  cancelled.findChild<QCheckBox*>(QStringLiteral("settingsVerticalClipboardBitmap"))->setChecked(false); cancelled.reject();
  require(cancelled.settings().vertical_clipboard_bitmap, "Cancelled clipboard rendering options applied");
}

void test_font_listing_policy() {
  namespace qt = jwpqt::qt;
  auto settings = qt::read_application_settings(
      "ShowAllFonts=false\nFuture=opaque\n");
  require(!qt::ApplicationSettings{}.show_all_fonts &&
              !settings.show_all_fonts,
          "Show-all-fonts source default changed");
  const auto serialized = qt::write_application_settings(settings);
  require(serialized.find("ShowAllFonts = false") != std::string::npos &&
              serialized.find("Future=opaque") != std::string::npos &&
              qt::read_application_settings("all_fonts=true\n")
                  .show_all_fonts,
          "Show-all-fonts source keys did not roundtrip");
  bool invalid = false;
  try {
    (void)qt::read_application_settings(
        "ShowAllFonts=invalid\nall_fonts=true\n");
  } catch (const std::exception&) {
    invalid = true;
  }
  require(invalid, "Invalid earlier show-all-fonts value was ignored");

  qt::ApplicationSettingsDialog options(settings);
  auto* show_all = options.findChild<QCheckBox*>(
      QStringLiteral("settingsShowAllFonts"));
  auto* japanese = options.findChild<QComboBox*>(
      QStringLiteral("settingsFont0"));
  auto* ascii = options.findChild<QComboBox*>(
      QStringLiteral("settingsAsciiFont"));
  auto* print = options.findChild<QComboBox*>(
      QStringLiteral("settingsPrintFamily"));
  require(show_all && japanese && ascii && print &&
              !show_all->isChecked() &&
              japanese->count() == print->count() &&
              japanese->count() <= ascii->count(),
          "Recommended-font controls are missing or inconsistent");
  for (int i = 0; i < japanese->count(); ++i) {
    const QString family = japanese->itemText(i);
    require(QFontDatabase::writingSystems(family).contains(
                QFontDatabase::Japanese),
            "Recommended Japanese font list contains an unsupported family");
  }
  const QString unavailable = QStringLiteral("Unavailable Japanese Font");
  japanese->setEditText(unavailable);
  print->setEditText(unavailable);
  show_all->setChecked(true);
  require(japanese->count() == ascii->count() &&
              print->count() == ascii->count() &&
              japanese->currentText() == unavailable &&
              print->currentText() == unavailable,
          "Show-all-fonts did not expose every public family or preserve manual input");
  options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  require(options.settings().show_all_fonts,
          "Show-all-fonts option did not apply");

  qt::ApplicationSettingsDialog cancelled(settings);
  cancelled.findChild<QCheckBox*>(QStringLiteral("settingsShowAllFonts"))
      ->setChecked(true);
  cancelled.reject();
  require(!cancelled.settings().show_all_fonts,
          "Cancelled show-all-fonts option changed settings");

  QTemporaryDir directory;
  require(directory.isValid(), "Show-all-fonts persistence directory failed");
  const QString path = directory.filePath(QStringLiteral("settings.cfg"));
  const QString project_path = directory.filePath(QStringLiteral("fonts.jpr"));
  qt::MainWindow window;
  auto persisted = window.application_settings();
  persisted.show_all_fonts = true;
  require(window.load_application_settings(path) &&
              window.apply_application_settings(persisted) &&
              window.save_application_settings() &&
              window.save_project_path(project_path, false),
          "Could not persist show-all-fonts policy");
  qt::MainWindow restarted;
  require(restarted.load_application_settings(path) &&
              restarted.application_settings().show_all_fonts,
          "Settings restart lost show-all-fonts policy");
  qt::MainWindow project;
  require(project.open_project_path(project_path) &&
              project.application_settings().show_all_fonts,
          "JPR restore lost show-all-fonts policy");
}

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try { test_fallback_and_big(); test_bitmap(); test_vertical_and_color_bitmap(); test_font_listing_policy(); std::cout << "Font rendering tests passed\n"; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
