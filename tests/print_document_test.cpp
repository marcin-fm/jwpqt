// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string_view>

#include <QApplication>
#include <QFile>
#include <QPageLayout>
#include <QPageRanges>
#include <QPrinter>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextCursor>
#include <QPainter>
#include <QFontMetricsF>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QAction>
#include <QTimer>
#include <QPointer>
#include <QTextEdit>
#include <QPrintPreviewWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include "main_window.h"
#include "application_settings_dialog.h"
#include "file_io.h"
#include "text_bridge.h"
#include "jwpqt/core/jwp_text_codec.h"

#include "print_document.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void require_error(const std::function<void()>& operation,
                   std::string_view message) {
  try {
    operation();
  } catch (const jwpqt::qt::PrintDocumentError&) {
    return;
  }
  require(false, message);
}

void test_pdf_print(const QString& directory) {
  const QString path = directory + QStringLiteral("/document.pdf");
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  printer.setOutputFileName(path);

  jwpqt::core::JwpDocument layout;
  layout.margins = {0.5F, 0.75F, 1.0F, 1.25F};
  layout.landscape = true;
  QTextDocument source;
  source.setPlainText(QStringLiteral("Native print \u65e5\u672c\nSecond line"));
  source.setModified(true);

  jwpqt::qt::print_document(printer, source, &layout);
  require(source.isModified(), "Printing changed the source document state");
  require(printer.pageLayout().orientation() == QPageLayout::Landscape,
          "Printing did not apply JWP landscape orientation");
  const QMarginsF margins =
      printer.pageLayout().margins(QPageLayout::Inch);
  require(std::abs(margins.left() - 0.5) < 0.01 &&
              std::abs(margins.right() - 0.75) < 0.01 &&
              std::abs(margins.top() - 1.0) < 0.01 &&
              std::abs(margins.bottom() - 1.25) < 0.01,
          "Printing applied JWP margins in the wrong order");
  QFile output(path);
  require(output.open(QIODevice::ReadOnly), "PDF print output was not created");
  require(output.size() > 500 && output.read(4) == QByteArray("%PDF", 4),
          "PDF print output is incomplete");
}

void test_invalid_layout() {
  QPrinter printer;
  jwpqt::core::JwpDocument layout;
  layout.margins[0] = std::numeric_limits<float>::quiet_NaN();
  QTextDocument source;
  require_error(
      [&] { jwpqt::qt::print_document(printer, source, &layout); },
      "Printing accepted an invalid JWP margin");
  QPageLayout page(QPageSize(QPageSize::A4), QPageLayout::Portrait, {72, 72, 72, 72}, QPageLayout::Point);
  source.setPlainText(QString::fromUcs4(U"A\U0001f600B"));
  jwpqt::qt::PrintOptions options;
  options.selection = {{1, 2}};
  require_error([&] { jwpqt::qt::PrintLayout invalid(source, page, nullptr, options); },
                "Printing accepted a split Unicode scalar");
}

QByteArray bytes(const QString& path) {
  QFile file(path); require(file.open(QIODevice::ReadOnly), "Could not read PDF fixture"); return file.readAll();
}
QString pdf_text(const QString& path) {
  require(!QStandardPaths::findExecutable(QStringLiteral("pdftotext")).isEmpty(),
          "PDF verification requires pdftotext (poppler-utils)");
  QProcess process;
  process.start(QStringLiteral("pdftotext"), {QStringLiteral("-layout"), path, QStringLiteral("-")});
  require(process.waitForFinished(30000) && process.exitCode() == 0, "PDF text extraction failed");
  const QString result = QString::fromUtf8(process.readAllStandardOutput());
  require(!result.trimmed().isEmpty(), "A nonempty print fixture produced no PDF text");
  return result;
}

void test_layout_and_ranges(const QString& directory) {
  using namespace jwpqt;
  QTextDocument source;
  source.setDefaultFont(QFont(QStringLiteral("Noto Sans CJK JP"), 12));
  QTextCursor cursor(&source);
  cursor.insertText(QStringLiteral("FIRST \u65e5\u672c \u00a0\U0001f600"));
  QTextBlockFormat new_page; new_page.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
  cursor.insertBlock(new_page); cursor.insertText(QStringLiteral("SECOND"));
  cursor.insertBlock(new_page); cursor.insertText(QStringLiteral("THIRD"));
  source.setModified(true);
  const auto original = source.toRawText();
  const auto undo = source.availableUndoSteps();
  core::JwpDocument metadata;
  metadata.margins = {1, 1, 1, 1};
  auto raw = [](std::u32string_view value) { return core::encode_jwp_text(value, core::LegacyCodePage::k1252); };
  metadata.summary[2] = raw(U"AUTHOR");
  metadata.headers[0][0] = raw(U"ODD &P &A &N && &z");
  metadata.headers[1][0] = raw(U"EVEN &P");
  metadata.headers[2][2] = raw(U"FOOT &P &D &T");
  metadata.headers[3][2] = raw(U"EVENFOOT &P");
  metadata.separate_left_right_headers = true;
  metadata.suppress_first_page_headers = true;
  qt::PrintOptions options;
  options.font = source.defaultFont(); options.file_name = QStringLiteral("/documents/name.jwp");
  options.time = QDateTime(QDate(2024, 2, 3), QTime(14, 5));
  QPrinter printer(QPrinter::HighResolution);
  printer.setOutputFormat(QPrinter::PdfFormat);
  const QString path = directory + QStringLiteral("/pages.pdf");
  printer.setOutputFileName(path);
  qt::configure_printer_for_jwp(printer, metadata);
  qt::PrintLayout plan(source, printer.pageLayout(), &metadata, options);
  require(plan.page_count() == 3 && plan.text().startsWith(QStringLiteral("FIRST")) &&
              plan.text().contains(QChar(0xa0)), "Print pagination or Unicode text changed");
  QImage image(plan.page_size().toSize() * 2, QImage::Format_ARGB32); image.fill(Qt::white);
  { QPainter painter(&image); painter.scale(2, 2); plan.paint_page(painter, 3); }
  require(image.save(QDir::currentPath() + QStringLiteral("/print-horizontal.png")), "Print capture failed");
  qt::print_document(printer, source, &metadata, options);
  const QString complete = pdf_text(path);
  if (!complete.isEmpty()) {
    require(complete.contains(QStringLiteral("FIRST")) && complete.contains(QStringLiteral("SECOND")) &&
                complete.contains(QStringLiteral("THIRD")), "PDF omitted document pages");
    require(!complete.contains(QStringLiteral("ODD 1")) && complete.contains(QStringLiteral("EVEN 2")) &&
                complete.contains(QStringLiteral("ODD 3 AUTHOR name.jwp & &z")) &&
                complete.contains(QStringLiteral("FOOT 3 24/2/3 2:05 PM")), "PDF header/footer expansion or parity is wrong");
  }
  printer.setPrintRange(QPrinter::PageRange); printer.setFromTo(2, 3);
  printer.setPageOrder(QPrinter::LastPageFirst);
  qt::print_document(printer, source, &metadata, options);
  const auto reverse = pdf_text(path);
  if (!reverse.isEmpty()) require(!reverse.contains(QStringLiteral("FIRST")) && reverse.indexOf(QStringLiteral("THIRD")) <
      reverse.indexOf(QStringLiteral("SECOND")), "PDF page range or reverse order failed");
  const auto saved = bytes(path);
  QPageRanges disjoint; disjoint.addPage(1); disjoint.addPage(3);
  printer.setPageRanges(disjoint);
  qt::print_document(printer, source, &metadata, options);
  const auto sparse = pdf_text(path);
  require(sparse.contains(QStringLiteral("FIRST")) && sparse.contains(QStringLiteral("THIRD")) &&
      !sparse.contains(QStringLiteral("SECOND")), "Disjoint page ranges included an unrequested page");
  printer.setPrintRange(QPrinter::AllPages); printer.setCopyCount(2); printer.setCollateCopies(true);
  qt::print_document(printer, source, &metadata, options);
  auto copies = pdf_text(path);
  if (!copies.isEmpty()) require(copies.count(QStringLiteral("FIRST")) == 2 && copies.count(QStringLiteral("THIRD")) == 2,
                                 std::string("PDF copy count was ignored: ") + copies.toStdString());
  require(copies.indexOf(QStringLiteral("FIRST")) < copies.lastIndexOf(QStringLiteral("THIRD")),
          "Collated PDF copies were not complete document sets");
  printer.setCollateCopies(false);
  qt::print_document(printer, source, &metadata, options);
  copies = pdf_text(path);
  require(copies.lastIndexOf(QStringLiteral("THIRD")) < copies.indexOf(QStringLiteral("SECOND")) &&
      copies.lastIndexOf(QStringLiteral("SECOND")) < copies.indexOf(QStringLiteral("FIRST")),
      "Uncollated PDF copies did not repeat each page in the requested order");
  printer.setCopyCount(1); printer.setPrintRange(QPrinter::PageRange);
  // Re-establish the prior output before exercising non-destructive failures.
  QFile reset(path); require(reset.open(QIODevice::WriteOnly) && reset.write(saved) == saved.size(), "Could not reset PDF fixture"); reset.close();
  printer.setFromTo(9, 10);
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Out-of-range printing succeeded");
  require(bytes(path) == saved, "Invalid range destroyed the previous PDF");
  printer.setPrintRange(QPrinter::AllPages); printer.setPageOrder(QPrinter::FirstPageFirst);
  options.progress = [](int page, int) { return page == 0; };
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Print cancellation was ignored");
  require(bytes(path) == saved && printer.outputFileName() == path, "Cancelled printing replaced output or printer target");
  options.progress = [](int page, int count) { return page < count; };
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Final cancellation was ignored");
  require(bytes(path) == saved, "Final cancellation published a completed but unwanted PDF");
  options.progress = {};
  auto fields = metadata; fields.headers = {}; fields.suppress_first_page_headers = false;
  fields.summary = {raw(U"L"), raw(U"S"), raw(U"A"), raw(U"K"), raw(U"C")};
  fields.headers[0][0] = raw(U"&l &s &k &c &f &n &a && &x &");
  auto field_options = options; field_options.file_name = QStringLiteral("/d/n");
  qt::print_document(printer, source, &fields, field_options);
  require(pdf_text(path).contains(QStringLiteral("L S K C /d/n n A & &x &")),
          "Header fields, lowercase aliases or literal ampersands changed");
  require(reset.open(QIODevice::WriteOnly) && reset.write(saved) == saved.size(), "Could not restore header fixture");
  reset.close();
  metadata.headers[0][0] = raw(U"This header cannot fit"); metadata.margins[2] = 0;
  metadata.suppress_first_page_headers = false;
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Invalid header geometry was accepted");
  require(bytes(path) == saved, "Invalid header destroyed the previous PDF");
  require(source.toRawText() == original && source.isModified() && source.availableUndoSteps() == undo,
          "Printing mutated source text, modification or undo state");
}

void test_selection_and_vertical(const QString& directory) {
  using namespace jwpqt;
  QTextDocument source;
  source.setDefaultFont(QFont(QStringLiteral("Noto Sans CJK JP"), 12));
  source.setPlainText(QStringLiteral("BEFORE\nSELECTED \u65e5\u672c\u8a9e ABC office \u30fc\uff08\uff09\nAFTER"));
  QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat);
  const QString path = directory + QStringLiteral("/selection.pdf"); printer.setOutputFileName(path);
  core::JwpDocument metadata; metadata.margins = {1, 1, 1, 1}; metadata.vertical = true;
  qt::PrintOptions options; options.font = source.defaultFont(); options.selection = {{7, source.toPlainText().indexOf(QStringLiteral("\nAFTER"))}};
  printer.setPrintRange(QPrinter::Selection);
  qt::print_document(printer, source, &metadata, options);
  const auto text = pdf_text(path);
  if (!text.isEmpty()) require(text.contains(QStringLiteral("SELECTED")) &&
      text.normalized(QString::NormalizationForm_KC).contains(QStringLiteral("office")) &&
      !text.contains(QStringLiteral("BEFORE")) && !text.contains(QStringLiteral("AFTER")),
      std::string("Vertical selection leaked content or duplicated Latin glyphs: ") + text.toStdString());
  qt::PrintLayout vertical(source, printer.pageLayout(), &metadata, options);
  require(vertical.text().startsWith(QStringLiteral("SELECTED")) && !vertical.text().contains(QStringLiteral("AFTER")),
          "Selection layout does not own exactly the selected text");
  QImage image(vertical.page_size().toSize() * 2, QImage::Format_ARGB32); image.fill(Qt::white);
  { QPainter painter(&image); painter.scale(2, 2); vertical.paint_page(painter, 1); }
  require(image.save(QDir::currentPath() + QStringLiteral("/print-vertical.png")), "Vertical capture failed");
  metadata.vertical = false;
  qt::PrintLayout horizontal(source, printer.pageLayout(), &metadata, options);
  QImage unrotated(image.size(), image.format()); unrotated.fill(Qt::white);
  { QPainter painter(&unrotated); painter.scale(2, 2); horizontal.paint_page(painter, 1); }
  require(image != unrotated, "Vertical output did not rotate any Japanese glyphs");
  source.setPlainText(QStringLiteral("CHANGED"));
  require(vertical.text().startsWith(QStringLiteral("SELECTED")), "Print plan retained live document state");

  QTextDocument hanging;
  hanging.setDefaultFont(options.font);
  QTextCursor cursor(&hanging);
  auto format = cursor.blockFormat();
  format.setTextIndent(-QFontMetricsF(options.font).horizontalAdvance(QStringLiteral("\u3000")));
  cursor.setBlockFormat(format); cursor.insertText(QStringLiteral("HANGING"));
  options.selection.reset();
  qt::PrintLayout indented(hanging, printer.pageLayout(), nullptr, options);
  QImage indent_image(indented.page_size().toSize(), QImage::Format_ARGB32); indent_image.fill(Qt::white);
  { QPainter painter(&indent_image); indented.paint_page(painter, 1); }
  const auto body = printer.pageLayout().paintRect(QPageLayout::Point);
  bool hanging_ink = false;
  for (int y = body.top(); y < body.top() + 40; ++y)
    for (int x = body.left() - 30; x < body.left(); ++x)
      hanging_ink |= indent_image.pixelColor(x, y) != QColor(Qt::white);
  require(hanging_ink, "Printing clipped a hanging indent at the nominal text margin");
}

class PrintWindow final : public jwpqt::qt::MainWindow {
 public:
  std::function<bool(QPrinter&)> prompt;
  std::function<bool(QPrinter&)> setup;
  protected:
  bool prompt_for_print(QPrinter& printer) override { return prompt(printer); }
  bool prompt_for_printer_setup(QPrinter& printer) override { return setup(printer); }
};

void test_print_policies(const QString& directory) {
  using namespace jwpqt;
  qt::PrintOptions options; options.font = QFont(QStringLiteral("Noto Sans CJK JP"), 16);
  options.time = QDateTime(QDate(2024, 2, 29), QTime(12, 5));
  options.colors = true; options.color_policy.list_mode = core::KanjiListColorMode::kMatch;
  options.color_policy.list_color = {255, 0, 0}; options.color_policy.colorize_uncommon = true;
  options.color_policy.uncommon_color = {0, 180, 0}; options.color_list.add(0x3026);
  core::JwpDocument metadata; metadata.margins = {1,1,1,1};
  metadata.headers[0][0] = core::encode_jwp_text(U"\u611b &D &T");
  const std::string custom = "&Y-&M-&D";
  std::copy(custom.begin(), custom.end(), options.formatting.patterns[0].begin());
  options.formatting.patterns[0][custom.size()] = 0;
  QTextDocument source;
  source.setPlainText(qt::to_qstring(core::decode_jwp_text({0x3026,'A',0x5021})));
  const auto original = source.toRawText(); const auto undo = source.availableUndoSteps();
  QPageLayout page(QPageSize(QPageSize::A4), QPageLayout::Portrait, {72,72,72,72}, QPageLayout::Point);
  const auto render = [&] {
    qt::PrintLayout layout(source, page, &metadata, options);
    QImage image(layout.page_size().toSize(), QImage::Format_RGB32); image.fill(Qt::white);
    QPainter painter(&image); layout.paint_page(painter, 1); painter.end(); return image;
  };
  const auto colored = [](const QImage& image, bool red, int top, int bottom) {
    int count = 0;
    for (int y = top; y < std::min(bottom, image.height()); ++y) for (int x = 0; x < image.width(); ++x) {
      const auto c = image.pixelColor(x, y);
      count += red ? c.red() > c.green() + 40 && c.red() > c.blue() + 40
                   : c.green() > c.red() + 40 && c.green() > c.blue() + 40;
    }
    return count;
  };
  for (bool vertical : {false, true}) {
    metadata.vertical = vertical;
    auto image = render();
    require(colored(image, true, 0, 72) > 0 && colored(image, true, 72, image.height()) > 0 &&
                colored(image, false, 72, image.height()) > 0, "Print list/header/uncommon colors missing");
    options.colors = false; auto mono = render();
    require(colored(mono, true, 0, mono.height()) == 0 && colored(mono, false, 0, mono.height()) == 0, "Disabled print colors leaked");
    options.colors = true; options.color_policy.list_mode = core::KanjiListColorMode::kOff;
    mono = render(); require(colored(mono, false, 0, mono.height()) == 0, "Uncommon printing ignored list-mode gate");
    options.color_policy.list_mode = core::KanjiListColorMode::kNoMatch;
    image = render(); require(colored(image, false, 0, image.height()) == 0 && colored(image, true, 72, image.height()) > 0,
        "Print list precedence failed");
    options.color_policy.list_mode = core::KanjiListColorMode::kMatch;
  }
  metadata.vertical = false;
  options.selection = {{1, 2}};
  const auto partial = render();
  require(colored(partial, true, 72, partial.height()) == 0 && colored(partial, false, 72, partial.height()) == 0 &&
      colored(partial, true, 0, 72) > 0, "Selection printing colored unselected body text or lost header colors");
  options.selection.reset();
  QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat);
  const auto path = directory + QStringLiteral("/policies.pdf"); printer.setOutputFileName(path);
  qt::print_document(printer, source, &metadata, options);
  require(pdf_text(path).contains(QStringLiteral("2024-2-29 12:05 AM")), "Custom date/time pattern missing from PDF");
  QProcess raster;
  const auto png = directory + QStringLiteral("/policy-output");
  raster.start(QStringLiteral("pdftoppm"), {QStringLiteral("-singlefile"), QStringLiteral("-scale-to"),
      QStringLiteral("842"), QStringLiteral("-png"), path, png});
  require(raster.waitForFinished(30000) && raster.exitCode() == 0, "Colored PDF rasterization failed");
  const QImage actual(png + QStringLiteral(".png"));
  require(!actual.isNull() && colored(actual, true, 0, actual.height()) > 0 &&
      colored(actual, false, 0, actual.height()) > 0, "Actual PDF did not preserve kanji colors");
  require(actual.save(QDir::currentPath() + QStringLiteral("/print-policies.png")), "Print policy capture failed");
  const auto original_image = render(); options.formatting.position = {50, 25, 50, 50};
  require(render() != original_image, "Header position controls did not move output");
  options.formatting.position[2] = 1000;
  const auto saved = bytes(path);
  require_error([&] { qt::print_document(printer, source, &metadata, options); }, "Invalid header geometry accepted");
  require(bytes(path) == saved && source.toRawText() == original && source.availableUndoSteps() == undo,
      "Print policy changed disk/source on failure");

  auto settings = qt::read_application_settings("colorkanji_print=true\nhead_left=50\nhead_top=75\n");
  settings.print_formatting.patterns[0][19] = 0xffff;
  const auto encoded = qt::write_application_settings(settings);
  const auto restored = qt::read_application_settings(encoded);
  require(restored.color_printing && restored.print_formatting.patterns == settings.print_formatting.patterns &&
      restored.print_formatting.position == settings.print_formatting.position && qt::write_application_settings(restored) == encoded,
      "Print configuration round trip lost raw JIS tail or units");
  bool failed = false;
  try { (void)qt::read_application_settings("head_top=bad\nhead_top=100\n"); } catch (const std::exception&) { failed = true; }
  require(failed, "Later header setting hid invalid input");
  failed = false;
  try { (void)qt::read_application_settings("Printing_Formatting_Date=" + std::string(80, '4') + "\n" + encoded); }
  catch (const std::exception&) { failed = true; }
  require(failed, "Later valid pattern hid an unterminated earlier array");
  auto unavailable = settings;
  unavailable.print_formatting.patterns[2][0] = 0x80; // Undefined in recovered CP1252.
  qt::ApplicationSettingsDialog retained(unavailable);
  require(retained.findChild<QLineEdit*>(QStringLiteral("settingsPrintPattern2"))->isReadOnly(),
      "Undisplayable legacy pattern did not retain its bytes safely");
  QMetaObject::invokeMethod(retained.findChild<QDialogButtonBox*>(), "accepted", Qt::DirectConnection);
  require(retained.settings().print_formatting.patterns == unavailable.print_formatting.patterns,
      "Opening print Options rewrote an undisplayable pattern");
  qt::ApplicationSettingsDialog dialog(settings);
  dialog.show();
  dialog.findChild<QTabWidget*>()->setCurrentWidget(dialog.findChild<QWidget*>(QStringLiteral("settingsPrintScroll")));
  QApplication::processEvents();
  require(dialog.grab().save(QDir::currentPath() + QStringLiteral("/print-format-options.png")), "Print Options capture failed");
  dialog.findChild<QLineEdit*>(QStringLiteral("settingsPrintPattern0"))->setText(QStringLiteral("&Y"));
  dialog.findChild<QDoubleSpinBox*>(QStringLiteral("settingsPrintPosition2"))->setValue(1.25);
  dialog.findChild<QCheckBox*>(QStringLiteral("settingsPrintColors"))->setChecked(false);
  QMetaObject::invokeMethod(dialog.findChild<QDialogButtonBox*>(), "accepted", Qt::DirectConnection);
  require(!dialog.settings().color_printing && dialog.settings().print_formatting.position[2] == 125 &&
      dialog.settings().print_formatting.patterns[0][2] == 0 && dialog.settings().print_formatting.patterns[0][19] == 0xffff,
      "Print Options lost edits or preserved tail");
  qt::ApplicationSettingsDialog cancelled(settings);
  cancelled.findChild<QCheckBox*>(QStringLiteral("settingsPrintColors"))->setChecked(false);
  cancelled.reject();
  require(cancelled.settings().color_printing, "Cancelled print preferences were applied");

  auto native = metadata; native.paragraphs.resize(1); native.paragraphs[0].text = {0x3026, 'A', 0x5021};
  const auto native_path = directory + QStringLiteral("/policy.jwp"); qt::write_jwp_file(native_path, native);
  PrintWindow window;
  const auto config = directory + QStringLiteral("/print-settings.cfg");
  require(window.load_application_settings(config) && window.open_jwp_path(native_path, core::LegacyCodePage::k1252, qt::OpenMode::kNonInteractive),
      "Print controller fixture failed");
  auto accepted = settings; accepted.print_formatting.patterns[0] = {'B','E','F','O','R','E',0};
  accepted.print_formatting.patterns[0].resize(20);
  require(window.load_kanji_color_configuration(directory + QStringLiteral("/print-colors.ini"),
      directory + QStringLiteral("/print-colors.txt"), qt::OpenMode::kNonInteractive) &&
      window.apply_application_settings(accepted) && window.save_application_settings() &&
      window.set_kanji_color_list(options.color_list, qt::OpenMode::kNonInteractive) &&
      window.set_kanji_color_policy(options.color_policy, qt::OpenMode::kNonInteractive), "Print policies could not be applied");
  qt::MainWindow restarted;
  require(restarted.load_application_settings(config) && restarted.application_settings().color_printing &&
      restarted.application_settings().print_formatting.patterns == accepted.print_formatting.patterns,
      "Print preferences did not survive restart");
  const auto model_before = *window.current_jwp_document();
  const auto captured_path = directory + QStringLiteral("/captured-policy.pdf");
  window.prompt = [&](QPrinter& device) {
    auto changed = accepted; changed.color_printing = false;
    changed.print_formatting.patterns[0][0] = 'X';
    require(window.apply_application_settings(changed), "Could not change preferences during print prompt");
    require(window.set_kanji_color_policy({}, qt::OpenMode::kNonInteractive), "Could not change live color policy");
    device.setOutputFormat(QPrinter::PdfFormat); device.setOutputFileName(captured_path); return true;
  };
  window.findChild<QAction*>(QStringLiteral("printAction"))->trigger();
  require(pdf_text(captured_path).contains(QStringLiteral("BEFORE")) && *window.current_jwp_document() == model_before,
      "Print snapshot changed with later formatting preferences or changed the document");
  raster.start(QStringLiteral("pdftoppm"), {QStringLiteral("-singlefile"), QStringLiteral("-scale-to"),
      QStringLiteral("842"), QStringLiteral("-png"), captured_path, png});
  require(raster.waitForFinished(30000) && raster.exitCode() == 0, "Captured PDF rasterization failed");
  const QImage captured(png + QStringLiteral(".png"));
  require(colored(captured, true, 0, captured.height()) > 0, "Modal print lost its captured color policy");
}

void test_window_workflow(const QString& directory) {
  using namespace jwpqt;
  const QString source_path = directory + QStringLiteral("/snapshot.txt");
  qt::write_text_file(source_path, {U"ORIGINAL CONTENT", core::TextEncoding::kUtf8, false});
  PrintWindow window;
  require(window.open_path(source_path, core::TextEncoding::kUtf8), "Could not load print snapshot");
  auto* editor = window.findChild<QTextEdit*>();
  auto* action = window.findChild<QAction*>(QStringLiteral("printAction"));
  require(editor && action, "Print command unavailable");
  QTextCursor selected(editor->document()); selected.setPosition(9); selected.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  editor->setTextCursor(selected);
  const QString path = directory + QStringLiteral("/snapshot.pdf");
  window.prompt = [&](QPrinter& printer) {
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(path); printer.setPrintRange(QPrinter::Selection);
    editor->selectAll(); editor->insertPlainText(QStringLiteral("EXTERNAL CHANGE"));
    action->trigger(); // A print prompt cannot recursively start a second job.
    return true;
  };
  action->trigger();
  const auto text = pdf_text(path);
  if (!text.isEmpty()) require(text.contains(QStringLiteral("CONTENT")) && !text.contains(QStringLiteral("ORIGINAL")) &&
      !text.contains(QStringLiteral("EXTERNAL")), "Modal printing did not use its frozen selection");
  require(editor->toPlainText() == QStringLiteral("EXTERNAL CHANGE"), "Printing reverted a newer editor change");
  window.prompt = [&](QPrinter& printer) { printer.setOutputFileName(source_path); return true; };
  const auto source_bytes = bytes(source_path);
  action->trigger(); require(bytes(source_path) == source_bytes, "Printing overwrote an open document");

  const QString late_path = directory + QStringLiteral("/opened-during-print.txt");
  qt::write_text_file(late_path, {U"KEEP THIS FILE", core::TextEncoding::kUtf8, false});
  const auto late_bytes = bytes(late_path);
  window.prompt = [&](QPrinter& printer) {
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(late_path);
    printer.setPrintRange(QPrinter::AllPages);
    QTimer::singleShot(0, &window, [&] {
      require(window.open_path(late_path, core::TextEncoding::kUtf8), "Could not open concurrent print target");
    });
    return true;
  };
  action->trigger();
  require(bytes(late_path) == late_bytes, "Printing overwrote a destination opened during progress");
  require(window.open_path(source_path, core::TextEncoding::kUtf8), "Could not restore preview source");
  editor = window.findChild<QTextEdit*>();
  editor->selectAll(); editor->insertPlainText(QStringLiteral("EXTERNAL CHANGE"));

  QTimer timer;
  bool preview_seen = false;
  QObject::connect(&timer, &QTimer::timeout, [&] {
    auto* preview = window.findChild<QDialog*>(QStringLiteral("printPreviewDialog"));
    if (!preview) return;
    require(preview->findChild<QPrintPreviewWidget*>()->pageCount() > 0 &&
        preview->findChild<QPushButton*>(QStringLiteral("printFromPreview"))->isEnabled(), "Preview rendering failed");
    preview_seen = true; timer.stop();
    preview->grab().save(QDir::currentPath() + QStringLiteral("/print-preview.png"));
    preview->reject();
  });
  timer.start(100);
  window.findChild<QAction*>(QStringLiteral("printPreviewAction"))->trigger();
  require(preview_seen && editor->toPlainText() == QStringLiteral("EXTERNAL CHANGE"), "Preview failed or changed the editor");
  require(bytes(source_path) == source_bytes && bytes(late_path) == late_bytes,
          "Preview wrote to a remembered printer destination");

  const QString preview_output = directory + QStringLiteral("/from-preview.pdf");
  window.prompt = [&](QPrinter& printer) {
    printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(preview_output);
    printer.setPrintRange(QPrinter::AllPages); return true;
  };
  QObject::disconnect(&timer, nullptr, nullptr, nullptr);
  QObject::connect(&timer, &QTimer::timeout, [&] {
    auto* preview = window.findChild<QDialog*>(QStringLiteral("printPreviewDialog"));
    if (!preview) return;
    timer.stop();
    editor->selectAll(); editor->insertPlainText(QStringLiteral("CHANGED WHILE PREVIEWING"));
    preview->findChild<QPushButton*>(QStringLiteral("printFromPreview"))->click();
  });
  timer.start(100);
  window.findChild<QAction*>(QStringLiteral("printPreviewAction"))->trigger();
  const auto preview_text = pdf_text(preview_output);
  require(preview_text.contains(QStringLiteral("EXTERNAL CHANGE")) &&
      !preview_text.contains(QStringLiteral("CHANGED WHILE")) &&
      editor->toPlainText() == QStringLiteral("CHANGED WHILE PREVIEWING"),
      "Print from preview discarded its snapshot or reverted a newer edit");

  auto* doomed = new PrintWindow;
  QPointer<PrintWindow> weak(doomed);
  doomed->prompt = [doomed](QPrinter&) { delete doomed; return true; };
  doomed->findChild<QAction*>(QStringLiteral("printAction"))->trigger();
  require(!weak, "Print owner deletion test failed");

  auto settings = qt::read_application_settings("print_font.size=95\nprint_font.automatic=false\nprint_font.name=\"Noto Sans CJK JP\"\n");
  require(settings.print_font.size == 95 && !settings.print_font.automatic &&
      qt::read_application_settings(qt::write_application_settings(settings)).print_font.size == 95,
      "Print font did not preserve tenths of a point");
  require(window.apply_application_settings(settings), "Print settings could not be applied");
  qt::ApplicationSettingsDialog options(settings);
  auto* size = options.findChild<QDoubleSpinBox*>(QStringLiteral("settingsPrintSize"));
  auto* automatic = options.findChild<QCheckBox*>(QStringLiteral("settingsPrintAutomatic"));
  require(size && automatic && size->value() == 9.5, "Printing settings controls missing");
  size->setValue(14.2); automatic->setChecked(true);
  QMetaObject::invokeMethod(options.findChild<QDialogButtonBox*>(), "accepted", Qt::DirectConnection);
  require(options.settings().print_font.size == 142 && options.settings().print_font.automatic,
          "Printing settings acceptance lost physical units");
  bool rejected = false;
  try { (void)qt::read_application_settings("Print.Size=bad\nprint_font.size=120\n"); } catch (const std::exception&) { rejected = true; }
  require(rejected, "A later print size hid an invalid earlier setting");

  const auto before_setup = *window.current_jwp_document();
  window.setup = [&](QPrinter& printer) {
    auto layout = printer.pageLayout(); layout.setUnits(QPageLayout::Inch);
    layout.setOrientation(QPageLayout::Landscape);
    require(layout.setMargins({0.5, 0.75, 1.25, 1.5}) && printer.setPageLayout(layout), "Invalid setup fixture");
    return true;
  };
  window.findChild<QAction*>(QStringLiteral("printerSetupAction"))->trigger();
  const auto configured = *window.current_jwp_document();
  require(configured.landscape && configured.margins == std::array<float, 4>{0.5, 1.25, 0.75, 1.5},
          "Printer Setup lost accepted document margins or orientation");
  window.findChild<QAction*>(QStringLiteral("undoAction"))->trigger();
  require(*window.current_jwp_document() == before_setup, "Printer Setup was not one undoable metadata change");
  window.setup = [&](QPrinter&) {
    require(window.open_path(late_path, core::TextEncoding::kUtf8), "Could not change setup target");
    return true;
  };
  window.findChild<QAction*>(QStringLiteral("printerSetupAction"))->trigger();
  require(window.current_jwp_document()->margins == std::array<float, 4>{1, 1, 1, 1} &&
      !window.document_modified(), "Printer Setup applied stale settings to a replacement document");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QTemporaryDir directory(QStringLiteral("/srv/tmp/jwpqt-print-XXXXXX"));
  require(directory.isValid(), "Could not create print test directory");
  test_pdf_print(directory.path());
  test_invalid_layout();
  test_layout_and_ranges(directory.path());
  test_selection_and_vertical(directory.path());
  test_window_workflow(directory.path());
  test_print_policies(directory.path());
  return EXIT_SUCCESS;
}
