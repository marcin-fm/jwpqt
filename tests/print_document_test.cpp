// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string_view>

#include <QApplication>
#include <QFile>
#include <QPageLayout>
#include <QPrinter>
#include <QTemporaryDir>
#include <QTextDocument>

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
  layout.margins[0] = 0.0F;
  layout.vertical = true;
  require_error(
      [&] { jwpqt::qt::print_document(printer, source, &layout); },
      "Printing silently ignored vertical JWP layout");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QTemporaryDir directory(QStringLiteral("/srv/tmp/jwpqt-print-XXXXXX"));
  require(directory.isValid(), "Could not create print test directory");
  test_pdf_print(directory.path());
  test_invalid_layout();
  return EXIT_SUCCESS;
}
