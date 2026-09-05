// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_PRINT_DOCUMENT_H
#define JWPQT_QT_PRINT_DOCUMENT_H

#include <stdexcept>

#include "jwpqt/core/jwp_document.h"

class QPrinter;
class QTextDocument;

namespace jwpqt::qt {

class PrintDocumentError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

void configure_printer_for_jwp(QPrinter& printer,
                               const core::JwpDocument& document);
void print_document(QPrinter& printer, const QTextDocument& source,
                    const core::JwpDocument* jwp_document = nullptr);

}  // namespace jwpqt::qt

#endif
