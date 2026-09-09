// SPDX-License-Identifier: GPL-2.0-or-later
#include "application_settings.h"
#include "application_settings_dialog.h"
#include "japanese_fonts.h"
#include "jwp_editor.h"
#include "main_window.h"
#include "jwp_text_drawing.h"
#include "print_document.h"
#include "text_bridge.h"
#include "file_io.h"
#include "jwpqt/core/jis_unicode.h"
#include "jwpqt/core/character_font.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QTextBlock>
#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QLabel>
#include <QPushButton>
#include <QPainterPath>
#include <QPainter>
#include <QPrinter>
#include <QProcess>
#include <QTextDocument>
#include <QtEndian>
#include <QRawFont>
#include <QTemporaryDir>
#include <QTextLayout>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace jwpqt;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
QRawFont shaped(const QFont& font, const QString& text) {
  QTextLayout layout(text, font);
  layout.beginLayout(); auto line = layout.createLine(); line.setLineWidth(1000); layout.endLayout();
  require(!layout.glyphRuns().empty(), "No shaped glyphs");
  return layout.glyphRuns().front().rawFont();
}

void test_vertical_tables() {
  const auto fixture = [](bool delta, bool ranges, bool extension) {
    std::string data;
    const auto word = [&](unsigned value) { data += static_cast<char>(value >> 8); data += static_cast<char>(value); };
    for (unsigned value : {1, 0, 0, 10, 24, 1}) word(value);
    data += "vert";
    for (unsigned value : {8, 0, 1, 0, 1, 4}) word(value);
    word(extension ? 7 : 1);
    for (unsigned value : {0, 1, 8}) word(value);
    if (extension) for (unsigned value : {1, 1, 0, 8}) word(value);
    word(delta ? 1 : 2); word(delta ? 6 : 10);
    word(2);
    if (!delta) { word(12); word(13); }
    word(ranges ? 2 : 1); word(ranges ? 1 : 2);
    word(10); word(11); if (ranges) word(0);
    return data;
  };
  require(!core::vertical_glyph_substitutions({}, 100), "Absent GSUB was not optional");
  for (bool delta : {false, true}) for (bool ranges : {false, true}) for (bool extension : {false, true}) {
    const auto data = fixture(delta, ranges, extension);
    const auto result = core::vertical_glyph_substitutions(data, 100);
    require(result && *result == std::map<std::uint16_t, std::uint16_t>{{10, 12}, {11, 13}}, "Vertical GSUB fixture mapping differs");
    for (std::size_t size = 1; size < data.size(); ++size) {
      bool rejected = false;
      try { (void)core::vertical_glyph_substitutions(std::string_view(data).substr(0, size), 100); }
      catch (const std::invalid_argument&) { rejected = true; }
      require(rejected, "Truncated selected vertical table was accepted");
    }
    auto absent = data; absent.replace(12, 4, "vrt2");
    require(!core::vertical_glyph_substitutions(absent, 100), "Unrequested vertical feature was selected");
  }
  const auto data = fixture(false, false, false);
  auto first_feature = data;
  first_feature.insert(18, std::string("vert\xff\xff", 6));
  first_feature[11] = 2; first_feature[17] = 14; first_feature[9] = 30;
  require(core::vertical_glyph_substitutions(first_feature, 100) == core::vertical_glyph_substitutions(data, 100),
          "Vertical parser followed an unselected second feature");
  auto first_lookup = data;
  first_lookup.insert(24, 2, '\xff'); first_lookup[21] = 2; first_lookup[9] = 26;
  require(core::vertical_glyph_substitutions(first_lookup, 100) == core::vertical_glyph_substitutions(data, 100),
          "Vertical parser followed an unselected second lookup");
  auto negative_delta = fixture(true, false, false); negative_delta[40] = '\xff'; negative_delta[41] = '\xfe';
  require(core::vertical_glyph_substitutions(negative_delta, 100)->at(10) == 8, "Signed vertical glyph delta changed");
  for (const auto& mutation : std::vector<std::pair<int, unsigned>>{{7, 0}, {23, 1}, {29, 2}, {33, 0}, {37, 3}, {41, 1}, {49, 3}, {53, 10}}) {
    auto bad = data; bad.at(mutation.first) = static_cast<char>(mutation.second);
    bool rejected = false;
    try { (void)core::vertical_glyph_substitutions(bad, 100); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Malformed vertical offsets/type/count/coverage accepted");
  }
  bool rejected = false;
  try { (void)core::vertical_glyph_substitutions(data, 12); } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "Vertical output glyph bound not checked");
}

void test_vertical_native(const QFont& font) {
  QFont fallback(QStringLiteral("DejaVu Sans")); fallback.setPixelSize(27);
  const auto horizontal_family = shaped(fallback, QStringLiteral("\u611b")).familyName();
  const auto raw = QRawFont::fromFont(font);
  const auto gsub = raw.fontTable("GSUB"), maxp = raw.fontTable("maxp");
  const auto substitutions = core::vertical_glyph_substitutions(gsub.toStdString(), qFromBigEndian<quint16>(maxp.constData() + 4));
  const auto comma = raw.glyphIndexesForString(QStringLiteral("\u3001")).front();
  require(substitutions && substitutions->count(comma) && substitutions->at(comma) != comma, "Test font lacks a real vertical comma alternate");
  const auto alternate = qt::jwp_vertical_font(raw);
  fallback.setPixelSize(29);
  require(shaped(fallback, QStringLiteral("\u611b")).familyName() == horizontal_family,
          "Private vertical face stole horizontal fallback glyphs");
  require(alternate.isValid() && alternate.familyName().startsWith(QStringLiteral("JwpqtVertical-")) &&
      alternate.glyphIndexesForString(QStringLiteral("\u3001")).front() == substitutions->at(comma), "Vertical cmap did not preserve comma identity");
  require(alternate.pathForGlyph(substitutions->at(comma)) == raw.pathForGlyph(substitutions->at(comma)) &&
      alternate.fontTable("OS/2").mid(8, 2) == raw.fontTable("OS/2").mid(8, 2), "Vertical face changed original outline/license flags");
  QFont italic = font; italic.setItalic(true);
  const auto italic_raw = QRawFont::fromFont(italic), italic_vertical = qt::jwp_vertical_font(italic_raw);
  require(italic_vertical.pathForGlyph(substitutions->at(comma)) == italic_raw.pathForGlyph(substitutions->at(comma)),
          "Vertical alternates lost synthetic italic shaping");
  auto resized = raw; resized.setPixelSize(48);
  require(qt::jwp_vertical_font(resized).pixelSize() == 48 && qt::jwp_vertical_font(resized).familyName() == alternate.familyName(), "Vertical cache changed face identity or size");
  QTextLayout layout(QStringLiteral("\u3001"), font);
  layout.beginLayout(); auto line = layout.createLine(); line.setLineWidth(100); layout.endLayout();
  QImage actual(160, 160, QImage::Format_ARGB32_Premultiplied); actual.fill(Qt::white);
  QImage expected = actual;
  const QImage blank = actual;
  const QPointF origin(50, 50);
  { QPainter painter(&actual); painter.setPen(Qt::red); qt::draw_jwp_text_layout(painter, layout, layout.text(), origin, true, core::kDefaultLegacyCodePage); }
  { QPainter painter(&expected); painter.setPen(Qt::red);
    const auto center = origin + QPointF((line.cursorToX(0) + line.cursorToX(1)) / 2, line.y() + line.height() / 2);
    painter.translate(center); painter.rotate(-90); painter.translate(-center);
    for (auto run : line.glyphRuns()) { run.setGlyphIndexes({substitutions->at(comma)}); painter.drawGlyphRun(origin, run); }
  }
  require(actual == expected && actual != blank, "Vertical TrueType drawing does not match alternate and rotation");
  require(actual.save(QStringLiteral("truetype-vertical.png")), "Could not capture TrueType vertical rendering");
  QTemporaryDir directory;
  QTextDocument source;
  source.setPlainText(QStringLiteral("A12\u3001\u3002\uff08\uff09\u30fc\u3041\u611b"));
  const auto before = qt::document_plain_text(source);
  QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat);
  qt::PrintOptions options; options.font = font;
  for (bool vertical : {false, true}) {
    core::JwpDocument metadata; metadata.margins = {1, 1, 1, 1}; metadata.vertical = vertical;
    printer.setOutputFileName(directory.filePath(vertical ? QStringLiteral("vertical.pdf") : QStringLiteral("horizontal.pdf")));
    qt::print_document(printer, source, &metadata, options);
    QProcess inspect; inspect.start(QStringLiteral("pdftotext"), {QStringLiteral("-raw"), printer.outputFileName(), QStringLiteral("-")});
    require(inspect.waitForFinished(15000) && inspect.exitCode() == 0, "Could not extract vertical PDF text");
    auto text = QString::fromUtf8(inspect.readAllStandardOutput()); text.remove(QLatin1Char('\n')); text.remove(QLatin1Char('\f'));
    if (text != before) std::cerr << "PDF vertical=" << vertical << " expected=" << before.toUtf8().toHex().constData()
                                << " actual=" << text.toUtf8().toHex().constData() << '\n';
    require(text == before, "Vertical alternates lost original PDF Unicode");
    require(qt::document_plain_text(source) == before, "Vertical printing mutated the source document");
  }
}

void test_representation() {
  constexpr auto page = core::LegacyCodePage::k1253;
  const auto sigma = core::unicode_to_jis_x0208(U'\u03a3');
  require(sigma.has_value(), "JIS Greek fixture is missing");
  core::JwpDocument source; source.paragraphs.resize(1);
  source.paragraphs[0].text = {0xd3, *sigma};
  source.margins = {1, 1, 1, 1}; source.headers[0][0] = source.paragraphs[0].text;
  QTemporaryDir temporary;
  const auto path = temporary.filePath(QStringLiteral("greek.jwp"));
  qt::write_jwp_file(path, source);
  qt::MainWindow window;
  qt::ApplicationSettings settings;
  settings.fonts[0] = {QStringLiteral("Noto Sans CJK JP"), 24, false};
  settings.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kBitmap)] = {QStringLiteral("Noto Sans CJK JP"), 24, false};
  const auto ascii_family = QRawFont::fromFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont)).familyName();
  require(QFontDatabase::families().contains(ascii_family), "ASCII fixture family is unavailable");
  settings.ascii_font = {ascii_family, 0, false};
  require(window.apply_application_settings(settings) && window.open_jwp_path(path, page, qt::OpenMode::kNonInteractive), "Could not open aliased JWP glyphs");
  window.show(); QApplication::processEvents();
  auto* editor = window.active_editor();
  const QString text = QStringLiteral("\u03a3\u03a3");
  const auto verify = [&] {
    require(qt::document_plain_text(*editor->document()) == text && window.current_jwp_document()->paragraphs[0].text == source.paragraphs[0].text,
            "Font identity changed raw tokens or Unicode");
    (void)editor->document()->size();
    auto* layout = editor->document()->begin().layout();
    const auto raw = layout->glyphRuns(0, 1), jis = layout->glyphRuns(1, 1);
    require(!raw.empty() && !jis.empty() && raw.front().rawFont().familyName().startsWith(QStringLiteral("JwpqtAscii-Bytes-")) &&
            jis.front().rawFont().familyName() == QStringLiteral("Noto Sans CJK JP"), "Aliased Greek used the wrong actual font");
    const auto original = shaped(QFont(ascii_family), QStringLiteral("\u03a3"));
    auto sized = original; sized.setPixelSize(raw.front().rawFont().pixelSize());
    if (raw.front().rawFont().pathForGlyph(raw.front().glyphIndexes().front()) !=
        sized.pathForGlyph(sized.glyphIndexesForString(QStringLiteral("\u03a3")).front()))
      std::cerr << "raw=" << raw.front().rawFont().familyName().toStdString() << " pixel=" << raw.front().rawFont().pixelSize()
                << " glyph=" << raw.front().glyphIndexes().front() << " original=" << sized.familyName().toStdString()
                << " pixel=" << sized.pixelSize() << " glyph=" << sized.glyphIndexesForString(QStringLiteral("\u03a3")).front() << '\n';
    require(raw.front().rawFont().pathForGlyph(raw.front().glyphIndexes().front()) ==
            sized.pathForGlyph(sized.glyphIndexesForString(QStringLiteral("\u03a3")).front()), "Raw byte outline changed");
  };
  verify();
  for (int at : {0, 1}) {
    const auto copy = [&](bool vertical) {
      settings.vertical_clipboard_bitmap = vertical;
      require(window.apply_application_settings(settings), "Representation settings refresh failed");
      QTextCursor cursor(editor->document()); cursor.setPosition(at); cursor.setPosition(at + 1, QTextCursor::KeepAnchor); editor->setTextCursor(cursor);
      editor->copy();
      require(QApplication::clipboard()->text() == QStringLiteral("\u03a3"), "Glyph-aware copy changed text");
      return qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData());
    };
    const auto horizontal = copy(false), vertical = copy(true);
    require(!horizontal.isNull() && !vertical.isNull(), "Representation clipboard image missing");
    require(at == 0 ? horizontal == vertical : horizontal != vertical, "Raw/JIS vertical identity was ignored");
  }
  verify();
  for (bool vertical : {false, true}) {
    auto model = source; model.vertical = vertical;
    QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat);
    const auto pdf = temporary.filePath(vertical ? QStringLiteral("vertical.pdf") : QStringLiteral("horizontal.pdf"));
    printer.setOutputFileName(pdf);
    qt::PrintOptions options; options.code_page = page; options.font = editor->font(); options.font.setPointSizeF(16);
    qt::print_document(printer, *editor->document(), &model, options);
    QProcess extract; extract.start(QStringLiteral("pdftotext"), {QStringLiteral("-raw"), pdf, QStringLiteral("-")});
    require(extract.waitForFinished() && extract.exitCode() == 0, "Representation PDF extraction failed");
    QString actual = QString::fromUtf8(extract.readAllStandardOutput());
    actual.remove(QLatin1Char('\n')); actual.remove(QLatin1Char('\f')); actual.remove(QLatin1Char(' '));
    require(actual == text + text, "Body/header glyph identities changed PDF Unicode");
  }
  editor->moveCursor(QTextCursor::End); editor->insertPlainText(QStringLiteral("!"));
  window.findChild<QAction*>(QStringLiteral("undoAction"))->trigger(); verify();
  require(!editor->document()->isModified(), "Representation formatting changed saved baseline");
  auto bad = source; bad.paragraphs[0].text[0] = 'X';
  bool rejected = false;
  try { qt::apply_jwp_character_fonts(*editor->document(), bad, page); } catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "Invalid raw/display mapping was accepted"); verify();
  settings.fonts[0].size = 30;
  require(window.apply_application_settings(settings), "Identity font size refresh failed"); verify();
  require(window.set_japanese_editing(false, true), "Could not switch to unrestricted Unicode");
  require(qt::document_plain_text(*editor->document()) == text, "Engine switch changed aliased text");
  for (auto block = editor->document()->begin(); block.isValid(); block = block.next())
    for (auto it = block.begin(); !it.atEnd(); ++it)
      require(!it.fragment().charFormat().hasProperty(qt::kJwpCharacterKind), "Native identity leaked into Unicode editing");
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  try {
    test_vertical_tables();
    const QString japanese = QStringLiteral("Noto Sans CJK JP");
    require(QFontDatabase::families().contains(japanese), "Japanese test font is missing");
    QFont original(japanese); original.setPixelSize(24);
    const auto original_raw = QRawFont::fromFont(original);
    test_vertical_native(original);
    test_representation();
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
    auto tagged = tables;
    std::string tagged_name;
    const auto word = [&](unsigned value) { tagged_name += static_cast<char>(value >> 8); tagged_name += static_cast<char>(value); };
    for (unsigned value : {1U,1U,28U,3U,1U,0x8001U,13U,8U,0U,2U,10U,8U,10U,18U}) word(value);
    for (char c : std::string("COPYen-USja-JP")) word(static_cast<unsigned char>(c));
    tagged["name"] = tagged_name;
    const auto tagged_bytes = core::make_character_font(tagged, {{U'A', glyph}}, "JwpqtLanguageTest");
    QRawFont tagged_font(QByteArray::fromStdString(tagged_bytes), 24);
    require(tagged_font.isValid() && tagged_font.pathForGlyph(glyph) == original_raw.pathForGlyph(glyph) &&
        tagged_font.fontTable("OS/2").mid(8, 2) == original_raw.fontTable("OS/2").mid(8, 2), "Language-tagged font lost native outline or embedding flags");
    const auto output_name = tagged_font.fontTable("name");
    const auto read_word = [&](int at) { require(at >= 0 && at + 2 <= output_name.size(), "Short output name table"); return qFromBigEndian<quint16>(output_name.constData() + at); };
    const int records = read_word(2), storage = read_word(4), languages = 6 + records * 12;
    require(read_word(0) == 1 && read_word(languages) == 2 && read_word(10) == 0x8001 && read_word(12) == 13,
        "Font name format, language index or license record changed");
    require(output_name.mid(storage + read_word(16), read_word(14)) == QByteArray::fromStdString(tagged_name.substr(28, 8)),
        "Language-tagged license bytes changed");
    for (int i = 0; i < 2; ++i)
      require(output_name.mid(storage + read_word(languages + 4 + i * 4), read_word(languages + 2 + i * 4)) ==
          QByteArray::fromStdString(tagged_name.substr(36 + i * 10, 10)), "Font language tag data/order changed");
    for (std::size_t length = 0; length < tagged_name.size(); ++length) {
      auto bad = tagged; bad["name"].resize(length);
      bool failed = false;
      try { (void)core::make_character_font(bad, {{U'A', glyph}}, "JwpqtLanguageTest"); } catch (const std::invalid_argument&) { failed = true; }
      require(failed, "Truncated language-tagged name table was accepted");
    }
    for (const auto& mutation : std::vector<std::pair<int, unsigned>>{{1,2}, {5,18}, {11,2}, {19,3}, {21,9}, {23,255}, {27,255}}) {
      auto bad = tagged; bad["name"][mutation.first] = static_cast<char>(mutation.second);
      bool failed = false;
      try { (void)core::make_character_font(bad, {{U'A', glyph}}, "JwpqtLanguageTest"); } catch (const std::invalid_argument&) { failed = true; }
      require(failed, "Invalid native name language offset/count/index was accepted");
    }
    auto excessive_languages = tagged;
    excessive_languages["name"].resize(40028, '\0');
    for (int at : {20, 24}) {
      excessive_languages["name"][at] = static_cast<char>(0x9c);
      excessive_languages["name"][at + 1] = 0x40;
      excessive_languages["name"][at + 2] = excessive_languages["name"][at + 3] = 0;
    }
    bool language_budget_rejected = false;
    try { (void)core::make_character_font(excessive_languages, {{U'A', glyph}}, "JwpqtLanguageTest"); }
    catch (const std::invalid_argument&) { language_budget_rejected = true; }
    require(language_budget_rejected, "Repeated font language references exceeded the aggregate budget");
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
    for (int i = 0; i < picker->count(); ++i)
      require(!picker->itemText(i).startsWith(QStringLiteral("JwpqtAscii-")) &&
                  !picker->itemText(i).startsWith(QStringLiteral("JwpqtVertical-")), "Transient font identity exposed for persistence");
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
