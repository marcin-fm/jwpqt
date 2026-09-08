// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/raster_font.h"
#include "japanese_fonts.h"
#include "application_settings_dialog.h"
#include "main_window.h"
#include "jwp_editor.h"
#include "print_document.h"
#include "text_bridge.h"

#include <QApplication>
#include <QComboBox>
#include <QClipboard>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QMimeData>
#include <QRawFont>
#include <QPrinter>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTextLayout>
#include <QGlyphRun>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace jwpqt;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void put(std::string& bytes, std::size_t p, unsigned value) {
  bytes[p] = static_cast<char>(value); bytes[p + 1] = static_cast<char>(value >> 8);
}
std::string fixture(int width = 16, bool holes = false, bool padded = false) {
  const int stride = padded ? ((width + 15) / 16) * 2 : width / 8;
  const int size = stride * width;
  std::string bytes(64 + (holes ? 7802 : 6878) * size, '\xff');
  std::fill(bytes.begin(), bytes.begin() + 64, 0);
  put(bytes, 40, width); put(bytes, 42, width); put(bytes, 44, size);
  put(bytes, 48, 64); put(bytes, 52, holes); put(bytes, 54, 2); put(bytes, 56, 6);
  const std::size_t index = holes ? 1415 : 529;  // Love, JIS 3026.
  for (int y = 2; y < width - 2; ++y)
    bytes[64 + index * size + y * stride] = static_cast<char>(0xc3); // four ink pixels.
  return bytes;
}
template<class F> void rejects(F function) {
  bool failed = false;
  try { function(); } catch (const core::RasterFontError&) { failed = true; }
  require(failed, "Malformed font was accepted");
}
void test_font(const std::string& bytes) {
  const core::RasterFont font(bytes);
  require(font.glyph_index(0xffff) == font.glyph_index(0x2223), "Missing glyph did not use source fallback");
  const auto face = font.native_face("JwpqtRasterTest");
  std::uint32_t checksum = 0;
  for (std::size_t p = 0; p < face.size(); p += 4) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value = (value << 8) | static_cast<unsigned char>(face[p + i]);
    checksum += value;
  }
  require(checksum == 0xb1b0afbaU, "Native face checksum mismatch");
  QRawFont raw(QByteArray::fromStdString(face), font.height(), QFont::PreferNoHinting);
  require(raw.isValid(), "Qt rejected generated font tables");
  const auto ids = raw.glyphIndexesForString(QStringLiteral("\u611b"));
  require(ids.size() == 1 && ids.front() == font.glyph_index(0x3026) + 1, "Native cmap lost Love");
  require(raw.glyphIndexesForString(QStringLiteral("A")).front() == 0, "Raster face stole ASCII glyphs");
  const auto image = raw.alphaMapForGlyph(ids.front());
  require(!image.isNull(), "Native glyph produced no pixels");
  const auto outline = raw.pathForGlyph(ids.front());
  for (int y = 0; y < font.height(); ++y) for (int x = 0; x < font.width(); ++x)
    require(outline.contains(QPointF(x + font.spacing() / 4 + 0.5, y - font.height() + 0.5)) ==
                font.ink(font.glyph_index(0x3026), x, y), "Native outline changed a source bitmap pixel");
  image.save(QStringLiteral("raster-%1.png").arg(font.height()));
  const auto scaled = font.native_face("JwpqtRasterTest");
  require(scaled == face, "Native font generation is not deterministic");
}
int main(int argc, char** argv) {
  QApplication application(argc, argv);
  try {
    const auto bytes = fixture();
    core::RasterFont font(bytes);
    require(font.width() == 16 && font.glyph_count() == 6878 && font.glyph_index(0x7424) == 6877,
            "Packed raster header or JIS map incorrect");
    for (unsigned code = 0; code <= 65535; ++code)
      require(font.glyph_index(static_cast<core::JisCode>(code)) < font.glyph_count(), "JIS fallback escaped the glyph table");
    require(font.glyph_index(0x2330) == 147 && font.glyph_index(0x2421) == 209 &&
                font.glyph_index(0x2621) == 378 && font.glyph_index(0x5021) == 3490,
            "Packed kana/Greek/kanji ranges differ from source");
    require(font.ink(font.glyph_index(0x3026), 2, 3) && !font.ink(529, 0, 3), "Zero-bit ink was inverted");
    rejects([&] { font.ink(7000, 0, 0); }); rejects([&] { font.ink(0, -1, 0); });
    rejects([&] { font.native_face("not a PostScript name"); });
    rejects([&] { core::RasterFont bad(bytes.substr(0, bytes.size() - 1)); });
    rejects([&] { core::RasterFont bad(bytes + std::string(32, 0)); });
    for (const auto change : {std::pair<std::size_t,unsigned>{40,0},{42,65},{44,1},{48,0},{52,2},{54,65535},{56,65535}}) {
      auto broken = bytes; put(broken, change.first, change.second);
      rejects([&] { core::RasterFont bad(broken); });
    }
    for (const auto& sample : {bytes, fixture(24, true), fixture(24, true, true)}) test_font(sample);
    const core::RasterFont holey(fixture(24, true));
    require(holey.glyph_index(0x7421) == holey.glyph_index(0x2223), "Holey last-four fallback read beyond the font");
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create font test directory");
    QFile output(directory.filePath(QStringLiteral("test.f00")));
    require(output.open(QIODevice::WriteOnly) && output.write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size()), "Could not write font fixture");
    output.close();
    qt::MainWindow window;
    require(window.load_application_settings(directory.filePath(QStringLiteral("jwpqt.cfg"))), "Could not initialize font directory");
    auto settings = window.application_settings();
    auto& system = settings.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kSystem)];
    system.family = QStringLiteral("test.f00"); system.size = 42;
    require(window.apply_application_settings(settings) && window.application_settings_warning().isEmpty(), "Native raster preferences failed");
    const auto native = qt::japanese_font(window, qt::JapaneseFontRole::kFile);
    require(native.family().startsWith(QStringLiteral("JwpqtRaster-")) && native.pixelSize() == 16,
            "Native role did not use raster height/family");
    require(QRawFont::fromFont(native).supportsCharacter(0x611b), "Registered raster face cannot render Love");
    QTextLayout layout(QStringLiteral("\u611b"), native);
    layout.beginLayout(); layout.createLine().setLineWidth(100); layout.endLayout();
    require(!layout.glyphRuns().empty() && layout.glyphRuns().front().rawFont().familyName() == native.family(),
            "Japanese shaping silently substituted a different font");
    qt::ApplicationSettingsDialog options(settings, &window);
    auto* families = options.findChild<QComboBox*>(QStringLiteral("settingsFont0"));
    require(families && families->currentText() == QStringLiteral("test.f00"), "Options lost original raster path");
    for (int i = 0; i < families->count(); ++i)
      require(!families->itemText(i).startsWith(QStringLiteral("JwpqtRaster-")), "Private face identity was offered as a persistent family");
    const auto print_font = qt::japanese_print_font(native, {QStringLiteral("test.f00"), 240, false}, directory.path());
    require(print_font.family() == native.family() && print_font.pointSizeF() == 24,
            "Print raster face did not retain physical size");
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(directory.filePath(QStringLiteral("raster.pdf")));
    QTextDocument print_source;
    print_source.setPlainText(QStringLiteral("\u611b"));
    qt::PrintOptions print_options; print_options.font = print_font;
    qt::print_document(printer, print_source, nullptr, print_options);
    QProcess inspect;
    const auto prefix = directory.filePath(QStringLiteral("printed-raster"));
    inspect.start(QStringLiteral("pdftoppm"), {QStringLiteral("-png"), QStringLiteral("-singlefile"),
        QStringLiteral("-scale-to"), QStringLiteral("512"), printer.outputFileName(), prefix});
    require(inspect.waitForFinished(15000) && inspect.exitCode() == 0, "Raster PDF inspection failed");
    QImage page(prefix + QStringLiteral(".png"));
    require(!page.isNull(), "Raster PDF produced no page image");
    int ink_pixels = 0;
    for (int y = 0; y < page.height(); ++y) for (int x = 0; x < page.width(); ++x)
      if (qGray(page.pixel(x, y)) < 128) ++ink_pixels;
    require(ink_pixels > 4, "Restricted raster font did not paint into PDF");
    require(window.save_application_settings(), "Could not save raster settings");
    qt::MainWindow restored;
    require(restored.load_application_settings(directory.filePath(QStringLiteral("jwpqt.cfg"))) &&
                qt::japanese_font(restored, qt::JapaneseFontRole::kFile).family() == native.family(), "Raster settings restart failed");
    system.family = QStringLiteral("missing.f00");
    require(window.apply_application_settings(settings) && window.resource_report().contains(QStringLiteral("Raster font 'missing.f00'")), "Missing raster font fallback wasn't disclosed");
    for (int i = 1; i < argc; ++i) {
      QFile source(QString::fromLocal8Bit(argv[i]));
      require(source.open(QIODevice::ReadOnly), "Could not open optional real font");
      const auto real = source.readAll().toStdString();
      test_font(real);
      system.family = source.fileName();
      require(window.apply_application_settings(settings), "Could not apply actual raster file");
      const auto real_font = qt::japanese_font(window, qt::JapaneseFontRole::kFile);
      require(real_font.family().startsWith(QStringLiteral("JwpqtRaster-")) &&
                  real_font.pixelSize() == core::RasterFont(real).height(), "Actual raster file did not reach the native role");
    }
    system.family = QStringLiteral("test.f00");
    require(window.apply_application_settings(settings), "Could not restore fixture raster");
    window.active_editor()->insertPlainText(QStringLiteral("\u611b"));
    window.active_editor()->selectAll();
    const auto before = *window.current_jwp_document();
    window.active_editor()->copy();
    require(QApplication::clipboard()->mimeData()->hasImage() &&
                QApplication::clipboard()->text() == QStringLiteral("\u611b") &&
                *window.current_jwp_document() == before, "Raster clipboard changed source text or lost the image");
    std::cout << "Raster fonts passed\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
