// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/raster_font.h"
#include "japanese_fonts.h"
#include "application_settings_dialog.h"
#include "main_window.h"
#include "jwp_editor.h"
#include "jwp_text_drawing.h"
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
#include <QtEndian>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace jwpqt;
constexpr core::JisCode offset_codes[] = {
    0x2122,0x2123,0x2124,0x2125,0x2421,0x2423,0x2425,0x2427,0x2429,0x2443,
    0x2463,0x2465,0x2467,0x246e,0x2521,0x2523,0x2525,0x2527,0x2529,0x2543,
    0x2563,0x2565,0x2567,0x256e,0x2575,0x2576};
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
  const core::RasterFont mapping(bytes);
  for (auto code : offset_codes) {
    const auto glyph = mapping.glyph_index(code);
    for (int i = 0; i < 3; ++i) {
      const int x = 2 + (code + i * 3) % (width - 4);
      const int y = 2 + (code / 7 + i * 5) % (width - 4);
      auto& byte = bytes[64 + glyph * size + y * stride + x / 8];
      byte = static_cast<char>(static_cast<unsigned char>(byte) & ~(0x80U >> (x % 8)));
    }
  }
  return bytes;
}
template<class F> void rejects(F function) {
  bool failed = false;
  try { function(); } catch (const core::RasterFontError&) { failed = true; }
  require(failed, "Malformed font was accepted");
}
std::pair<int, int> reference_offset(const core::RasterFont& font, core::JisCode code) {
  if (std::find(std::begin(offset_codes), std::end(offset_codes), code) == std::end(offset_codes)) return {};
  const int width = font.width();
  std::vector<bool> rotated(static_cast<std::size_t>(width * width));
  for (int y = 0; y < width; ++y) for (int x = 0; x < width; ++x)
    rotated[static_cast<std::size_t>((width - x - 1) * width + y)] = font.ink(font.glyph_index(code), x, y);
  int top = 0, left = 0, right = width - 1;
  for (; top < width; ++top) {
    int x = 0; while (x < width && !rotated[static_cast<std::size_t>(top * width + x)]) ++x;
    if (x < width) break;
  }
  for (; left < width; ++left) {
    int y = 0; while (y < width && !rotated[static_cast<std::size_t>(y * width + left)]) ++y;
    if (y < width) break;
  }
  for (; right >= 0; --right) {
    int y = 0; while (y < width && !rotated[static_cast<std::size_t>(y * width + right)]) ++y;
    if (y < width) break;
  }
  return {left, top - (width - right - 1)};
}
void test_vertical_drawing(const core::RasterFont& source, const QFont& font) {
  QTextLayout layout(QStringLiteral("\u3041"), font);
  layout.beginLayout(); layout.createLine().setLineWidth(160); layout.endLayout();
  const auto line = layout.lineAt(0);
  const QPointF origin(50, 50);
  QImage actual(200, 200, QImage::Format_RGB32); actual.fill(Qt::white);
  { QPainter painter(&actual); painter.setPen(Qt::red);
    qt::draw_jwp_text_layout(painter, layout, layout.text(), origin, true, core::kDefaultLegacyCodePage); }
  QImage expected(actual.size(), actual.format()); expected.fill(Qt::white);
  const auto offset = reference_offset(source, 0x2421);
  const auto runs = line.glyphRuns(0, 1);
  require(!runs.empty() && !runs.front().rawFont().fontTable("JWPV").isEmpty(), "Vertical drawing did not use the raster face");
  const auto scale = runs.front().rawFont().pixelSize() / source.height();
  { QPainter painter(&expected); painter.setPen(Qt::red);
    painter.translate(-offset.first * scale, -offset.second * scale);
    const auto center = origin + QPointF((line.cursorToX(0) + line.cursorToX(1)) / 2,
                                        line.y() + line.height() / 2);
    painter.translate(center); painter.rotate(-90); painter.translate(-center);
    for (const auto& run : runs) painter.drawGlyphRun(origin, run); }
  require(actual == expected, "Native vertical raster output differs from source pixel offsets");
  int colored = 0;
  for (int y = 0; y < actual.height(); ++y) for (int x = 0; x < actual.width(); ++x)
    if (qGreen(actual.pixel(x, y)) < 128) ++colored;
  require(colored > 0 && offset != std::pair<int, int>{}, "Vertical positioning fixture has no meaningful ink/offset");
  require(actual.save(QStringLiteral("raster-vertical-%1.png").arg(source.height())), "Could not capture vertical raster");
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
  const auto metadata = raw.fontTable("JWPV");
  require(metadata.size() == 8 + 26 * 6, "Vertical raster metadata was not preserved by Qt");
  for (auto code : offset_codes) {
    require(font.vertical_offset(code) == reference_offset(font, code), "Raster offset differs from source matrix scans");
    bool found = false;
    for (int i = 0; i < 26; ++i) {
      const auto* data = metadata.constData() + 8 + i * 6;
      if (qFromBigEndian<quint16>(data) != code) continue;
      const int x = qFromBigEndian<quint16>(data + 2);
      const auto raw_y = qFromBigEndian<quint16>(data + 4);
      const int y = raw_y < 32768 ? raw_y : static_cast<int>(raw_y) - 65536;
      require(std::make_pair(x, y) == reference_offset(font, code), "Private font lost signed source offsets");
      found = true;
    }
    require(found, "Private font omitted a source offset glyph");
  }
  require(font.vertical_offset(0x3026) == std::pair<int, int>{} &&
              font.vertical_offset(0x213c) == std::pair<int, int>{}, "Unlisted glyph received a vertical offset");
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
    constexpr core::JisCode exceptions[] = {0x213b,0x213c,0x2141,0x2142,0x2143,0x2144,0x2145,
      0x214a,0x214b,0x214c,0x214d,0x214e,0x214f,0x2150,0x2151,0x2152,0x2153,0x2154,0x2155,
      0x2156,0x2157,0x2158,0x2159,0x215a,0x215b,0x2161,0x2162,0x2163,0x2164,0x2165,0x2166,0x2167,
      0x222a,0x222b,0x222e,0x2127};
    for (unsigned code = 0; code <= 65535; ++code)
      require(core::jwp_glyph_rotates(static_cast<core::JisCode>(code)) ==
                  (code >= 0x2100 && std::find(std::begin(exceptions), std::end(exceptions), code) == std::end(exceptions)),
              "Vertical rotation exceptions differ from source");
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
    auto extra = fixture(24, true);
    put(extra, 46, 4); extra.append(4 * 72, '\0');
    const core::RasterFont declared_vertical(extra);
    require(declared_vertical.glyph_count() == 7806 && declared_vertical.glyph_index(0x7424) == 7805 &&
                declared_vertical.ink(7805, 23, 23), "Unused vertical count hid physical standard glyphs");
    auto rectangular = fixture();
    put(rectangular, 42, 8); put(rectangular, 44, 16);
    rectangular.resize(64 + 6878 * 16);
    const core::RasterFont non_square(rectangular);
    require(!non_square.native_face("JwpqtRectangular").empty(), "Horizontal rectangular font was rejected");
    rejects([&] { (void)non_square.vertical_offset(0x2421); });
    auto empty = fixture();
    std::fill(empty.begin() + 64, empty.end(), '\xff');
    require(core::RasterFont(empty).vertical_offset(0x2421) == std::make_pair(16, 0),
            "Blank source glyph offset differs from its bounded scan");
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create font test directory");
    QFile output(directory.filePath(QStringLiteral("test.f00")));
    require(output.open(QIODevice::WriteOnly) && output.write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size()), "Could not write font fixture");
    output.close();
    qt::MainWindow window;
    require(window.load_application_settings(directory.filePath(QStringLiteral("jwpqt.cfg"))), "Could not initialize font directory");
    auto settings = window.application_settings();
    settings.omit_clipboard_bitmap = false;
    auto& system = settings.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kSystem)];
    system.family = QStringLiteral("test.f00"); system.size = 42;
    require(window.apply_application_settings(settings) && window.application_settings_warning().isEmpty(), "Native raster preferences failed");
    const auto native = qt::japanese_font(window, qt::JapaneseFontRole::kFile);
    const auto raster_family = native.families().last();
    require(raster_family.startsWith(QStringLiteral("JwpqtRaster-")) && native.pixelSize() == 16,
            "Native role did not use raster height/family");
    QFont raster_font(raster_family); raster_font.setPixelSize(native.pixelSize());
    require(QRawFont::fromFont(raster_font).supportsCharacter(0x611b), "Registered raster face cannot render Love");
    QTextLayout layout(QStringLiteral("\u611b"), native);
    layout.beginLayout(); layout.createLine().setLineWidth(100); layout.endLayout();
    require(!layout.glyphRuns().empty() && layout.glyphRuns().front().rawFont().familyName() == raster_family,
            "Japanese shaping silently substituted a different font");
    test_vertical_drawing(font, native);
    qt::ApplicationSettingsDialog options(settings, &window);
    auto* families = options.findChild<QComboBox*>(QStringLiteral("settingsFont0"));
    require(families && families->currentText() == QStringLiteral("test.f00"), "Options lost original raster path");
    for (int i = 0; i < families->count(); ++i)
      require(!families->itemText(i).startsWith(QStringLiteral("JwpqtRaster-")), "Private face identity was offered as a persistent family");
    const auto print_font = qt::japanese_print_font(native, {QStringLiteral("test.f00"), 240, false}, directory.path());
    require(print_font.families() == native.families() && print_font.pointSizeF() == 24,
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
    print_source.setPlainText(QStringLiteral("\u3041\u3001"));
    QImage horizontal_small;
    for (bool vertical : {false, true}) {
      core::JwpDocument metadata; metadata.margins = {1, 1, 1, 1}; metadata.vertical = vertical;
      printer.setOutputFileName(directory.filePath(vertical ? QStringLiteral("vertical-small.pdf") : QStringLiteral("horizontal-small.pdf")));
      qt::print_document(printer, print_source, &metadata, print_options);
      const auto small_prefix = directory.filePath(vertical ? QStringLiteral("vertical-small") : QStringLiteral("horizontal-small"));
      inspect.start(QStringLiteral("pdftoppm"), {QStringLiteral("-png"), QStringLiteral("-singlefile"),
          QStringLiteral("-scale-to"), QStringLiteral("512"), printer.outputFileName(), small_prefix});
      require(inspect.waitForFinished(15000) && inspect.exitCode() == 0, "Small-kana PDF inspection failed");
      QImage small(small_prefix + QStringLiteral(".png"));
      require(!small.isNull(), "Small-kana PDF has no image");
      int ink = 0;
      for (int y = 0; y < small.height(); ++y) for (int x = 0; x < small.width(); ++x)
        if (qGray(small.pixel(x, y)) < 128) ++ink;
      require(ink > 0, "Small-kana PDF contains no visible glyphs");
      if (!vertical) horizontal_small = small;
      else require(small != horizontal_small, "Vertical PDF ignored raster glyph positioning");
      require(qt::document_plain_text(print_source) == QStringLiteral("\u3041\u3001"), "Raster printing changed text");
    }
    require(window.save_application_settings(), "Could not save raster settings");
    qt::MainWindow restored;
    require(restored.load_application_settings(directory.filePath(QStringLiteral("jwpqt.cfg"))) &&
                qt::japanese_font(restored, qt::JapaneseFontRole::kFile).families() == native.families(), "Raster settings restart failed");
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
      require(real_font.families().last().startsWith(QStringLiteral("JwpqtRaster-")) &&
                  real_font.pixelSize() == core::RasterFont(real).height(), "Actual raster file did not reach the native role");
      test_vertical_drawing(core::RasterFont(real), real_font);
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
    auto& bitmap = settings.fonts[static_cast<std::size_t>(qt::JapaneseFontRole::kBitmap)];
    bitmap.family = QStringLiteral("test.f00");
    require(window.apply_application_settings(settings), "Could not select raster bitmap font");
    window.active_editor()->insertPlainText(QStringLiteral("\u3041\u3001"));
    window.active_editor()->selectAll();
    const auto small_before = *window.current_jwp_document();
    window.active_editor()->copy();
    const QImage horizontal_clipboard = qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData());
    settings.vertical_clipboard_bitmap = true;
    require(window.apply_application_settings(settings), "Could not enable vertical raster clipboard");
    window.active_editor()->copy();
    const QImage vertical_clipboard = qvariant_cast<QImage>(QApplication::clipboard()->mimeData()->imageData());
    require(!horizontal_clipboard.isNull() && !vertical_clipboard.isNull() && vertical_clipboard != horizontal_clipboard &&
                QApplication::clipboard()->text() == QStringLiteral("\u3041\u3001") &&
                *window.current_jwp_document() == small_before, "Vertical raster clipboard lost positioning or changed source data");
    std::cout << "Raster fonts passed\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
