// SPDX-License-Identifier: GPL-2.0-or-later

#include "print_document.h"

#include <cmath>
#include <memory>

#include <QMarginsF>
#include <QPageLayout>
#include <QPrinter>
#include <QTextDocument>

namespace jwpqt::qt {
namespace {

void validate_margin(float margin) {
  if (!std::isfinite(margin) || margin < 0.0F || margin > 10.0F)
    throw PrintDocumentError("JWP print margin is outside the supported range");
}

}  // namespace

void configure_printer_for_jwp(QPrinter& printer,
                               const core::JwpDocument& document) {
  if (document.vertical)
    throw PrintDocumentError("Vertical JWP printing is not implemented");
  for (const float margin : document.margins)
    validate_margin(margin);

  QPageLayout layout = printer.pageLayout();
  layout.setOrientation(document.landscape ? QPageLayout::Landscape
                                            : QPageLayout::Portrait);
  const QMarginsF inches(document.margins[0], document.margins[2],
                         document.margins[1], document.margins[3]);
  layout.setUnits(QPageLayout::Inch);
  if (!layout.setMargins(inches))
    throw PrintDocumentError("JWP print margins do not fit the selected page");
  if (!printer.setPageLayout(layout))
    throw PrintDocumentError("The selected printer rejected the JWP page layout");
}

void print_document(QPrinter& printer, const QTextDocument& source,
                    const core::JwpDocument* jwp_document) {
  if (jwp_document != nullptr)
    configure_printer_for_jwp(printer, *jwp_document);
  std::unique_ptr<QTextDocument> printable(source.clone());
  if (!printable)
    throw PrintDocumentError("Could not clone the document for printing");
  printable->print(&printer);
}

}  // namespace jwpqt::qt
