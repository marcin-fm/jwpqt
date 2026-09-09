// SPDX-License-Identifier: GPL-2.0-or-later
#include "application_settings.h"
#include "application_settings_dialog.h"
#include "japanese_fonts.h"
#include "jwp_editor.h"
#include "main_window.h"
#include "jwpqt/core/character_font.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QLabel>
#include <QPushButton>
#include <QPainterPath>
#include <QRawFont>
#include <QTemporaryDir>
#include <QTextLayout>
#include <iostream>
#include <stdexcept>

using namespace jwpqt;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
QRawFont shaped(const QFont& font, const QString& text) {
  QTextLayout layout(text, font);
  layout.beginLayout(); auto line = layout.createLine(); line.setLineWidth(1000); layout.endLayout();
  require(!layout.glyphRuns().empty(), "No shaped glyphs");
  return layout.glyphRuns().front().rawFont();
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  try {
    const QString japanese = QStringLiteral("Noto Sans CJK JP");
    require(QFontDatabase::families().contains(japanese), "Japanese test font is missing");
    QFont original(japanese); original.setPixelSize(24);
    const auto original_raw = QRawFont::fromFont(original);
    require(original_raw.supportsCharacter('A') && original_raw.supportsCharacter(0x611b), "Test must use a font covering BOTH ASCII and Japanese");
    std::map<std::string, std::string> tables;
    for (const char* tag : {"head", "hhea", "maxp", "hmtx", "name", "OS/2", "post", "glyf", "loca", "CFF "}) {
      const auto data = original_raw.fontTable(tag);
      if (!data.isEmpty()) tables.emplace(tag, data.toStdString());
    }
    const auto glyph = original_raw.glyphIndexesForString(QStringLiteral("A")).front();
    const auto bytes = core::make_character_font(tables, {{U'A', glyph}}, "JwpqtCharacterTest");
    QRawFont restricted(QByteArray::fromStdString(bytes), 24);
    require(restricted.isValid() && restricted.supportsCharacter('A') && !restricted.supportsCharacter(0x611b), "Restricted native cmap leaked Japanese coverage");
    require(restricted.fontTable("OS/2").mid(8, 2) == original_raw.fontTable("OS/2").mid(8, 2), "Font embedding or license flags changed");
    require(restricted.pathForGlyph(glyph) == original_raw.pathForGlyph(glyph), "Character restriction changed outlines");
    for (const auto& mapping : {std::map<char32_t, std::uint32_t>{{0xd800, glyph}}, {{U'A', 0}}, {{U'A', 0xffffffff}}}) {
      bool rejected = false;
      try { (void)core::make_character_font(tables, mapping, "JwpqtCharacterTest"); } catch (const std::invalid_argument&) { rejected = true; }
      require(rejected, "Invalid restricted mapping was accepted");
    }
    auto malformed = tables; malformed["name"] = "short";
    bool rejected = false;
    try { (void)core::make_character_font(malformed, {{U'A', glyph}}, "JwpqtCharacterTest"); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Malformed font name was accepted");
    std::string substitutions(28, '\0');
    substitutions[1] = 1; substitutions[5] = 10; substitutions[7] = 12; substitutions[9] = 26;
    substitutions[13] = 1; substitutions.replace(14, 4, "liga");
    substitutions[19] = 8; substitutions[23] = 1;
    auto layout_tables = tables; layout_tables["GSUB"] = substitutions;
    QRawFont unligated(QByteArray::fromStdString(core::make_character_font(layout_tables, {{U'A', glyph}}, "JwpqtNoLigature")), 24);
    require(unligated.isValid() && unligated.fontTable("GSUB").mid(22, 2) == QByteArray(2, '\0'),
            "Optional Latin ligature lookup survived character-by-character restriction");
    layout_tables["GSUB"][19] = 127;
    rejected = false;
    try { (void)core::make_character_font(layout_tables, {{U'A', glyph}}, "JwpqtNoLigature"); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Out-of-bounds font feature was accepted");

    qt::ApplicationSettings settings;
    settings.fonts[0] = {japanese, 24, false};
    settings.ascii_font = {japanese, 0, true};
    QWidget owner; QLabel label(&owner);
    qt::assign_japanese_font(label, qt::JapaneseFontRole::kFile);
    require(qt::set_japanese_fonts(owner, settings).empty(), "ASCII font resolution reported a warning");
    const auto font = label.font();
    const auto latin = shaped(font, QStringLiteral("A")), kanji = shaped(font, QStringLiteral("\u611b"));
    require(latin.familyName().startsWith(QStringLiteral("JwpqtAscii-")) && !latin.supportsCharacter(0x611b), "ASCII font not independently restricted");
    require(kanji.familyName() == original_raw.familyName() && latin.pixelSize() == kanji.pixelSize() && kanji.pixelSize() == 24,
            "Japanese glyph font or matched ASCII height changed");
    require(latin.pathForGlyph(glyph) == original_raw.pathForGlyph(glyph), "Actual ASCII shaping changed original glyph geometry");
    for (int size : {16, 32}) {
      settings.fonts[0].size = size; qt::set_japanese_fonts(owner, settings);
      require(shaped(label.font(), QStringLiteral("A")).pixelSize() == size && shaped(label.font(), QStringLiteral("\u611b")).pixelSize() == size,
              "ASCII did not follow Japanese role height");
    }
    const auto parsed = qt::read_application_settings("ascii_font.name = \"Droid Sans Fallback\"\nASCII.Size = 0\nASCII.Auto = true\nUnknown_Font = value\n");
    const auto encoded = qt::write_application_settings(parsed);
    require(qt::write_application_settings(qt::read_application_settings(encoded)) == encoded && encoded.find("Unknown_Font = value") != std::string::npos,
            "ASCII settings did not preserve aliases/unknown entries");
    rejected = false;
    try { (void)qt::read_application_settings("ASCII.Size = -1\nASCII.Size = 0\n"); } catch (const std::exception&) { rejected = true; }
    require(rejected, "An invalid earlier ASCII size was ignored");
    qt::ApplicationSettingsDialog dialog(settings);
    auto* picker = dialog.findChild<QComboBox*>(QStringLiteral("settingsAsciiFont"));
    require(picker && picker->currentText() == japanese, "ASCII Options control missing");
    for (int i = 0; i < picker->count(); ++i) require(!picker->itemText(i).startsWith(QStringLiteral("JwpqtAscii-")), "Transient ASCII identity exposed for persistence");
    picker->setEditText(QStringLiteral("DejaVu Sans Mono"));
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    require(dialog.settings().ascii_font.family == QStringLiteral("DejaVu Sans Mono"), "ASCII Options did not apply");
    qt::ApplicationSettingsDialog cancelled(settings);
    cancelled.findChild<QComboBox*>(QStringLiteral("settingsAsciiFont"))->setEditText(QStringLiteral("DejaVu Sans Mono"));
    cancelled.reject();
    require(cancelled.settings().ascii_font.family == japanese, "Cancelled ASCII selection applied");
    auto missing = settings; missing.ascii_font.family = QStringLiteral("Jwpqt-Missing-ASCII-Font");
    require(!qt::set_japanese_fonts(owner, missing).empty() &&
                shaped(label.font(), QStringLiteral("A")).familyName().startsWith(QStringLiteral("JwpqtAscii-")) &&
                shaped(label.font(), QStringLiteral("\u611b")).familyName() == original_raw.familyName(),
            "Missing ASCII family lost its warning, fallback or Japanese face");
    qt::set_japanese_fonts(owner, settings);
    QTemporaryDir temporary;
    qt::MainWindow window;
    const auto path = temporary.filePath(QStringLiteral("jwpqt.cfg"));
    require(window.load_application_settings(path) && window.apply_application_settings(settings) && window.save_application_settings(), "ASCII settings persistence failed");
    window.active_editor()->insertPlainText(QStringLiteral("A\u611b"));
    const auto before = *window.current_jwp_document();
    const auto cursor = window.active_editor()->textCursor();
    const auto changed = dialog.settings();
    require(window.apply_application_settings(changed) && *window.current_jwp_document() == before &&
            window.active_editor()->textCursor().position() == cursor.position(), "ASCII font changed document or selection");
    window.findChild<QAction*>(QStringLiteral("undoAction"))->trigger();
    require(window.active_editor()->document()->isEmpty(), "ASCII font application damaged native undo");
    qt::MainWindow restored;
    require(restored.load_application_settings(path) && restored.application_settings().ascii_font.family == japanese,
            "Original ASCII font name did not survive restart");
    std::cout << "ASCII font tests passed\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
