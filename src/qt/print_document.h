// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_PRINT_DOCUMENT_H
#define JWPQT_QT_PRINT_DOCUMENT_H

#include <stdexcept>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <QDateTime>
#include <QFont>
#include <QPageLayout>
#include <QString>

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/legacy_code_page.h"
#include "jwpqt/core/kanji_color_list.h"
#include "jwpqt/core/print_format.h"

class QPrinter;
class QTextDocument;
class QPainter;

namespace jwpqt::qt {

class PrintDocumentError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct PrintOptions {
  QFont font;
  QString file_name;
  core::LegacyCodePage code_page = core::LegacyCodePage::k1252;
  std::optional<std::pair<int, int>> selection;
  QDateTime time = QDateTime::currentDateTime();
  core::JwpPrintFormatting formatting;
  core::LegacyCodePage format_code_page = core::kDefaultLegacyCodePage;
  bool colors = false;
  core::KanjiColorList color_list;
  core::KanjiColorPolicy color_policy;
  bool preview = false;
  std::function<bool(int, int)> progress;
};

// Owns all text and layout data; drawing never accesses the live editor.
class PrintLayout {
 public:
  PrintLayout(const QTextDocument& source, const QPageLayout& page,
              const core::JwpDocument* jwp = nullptr, PrintOptions options = {});
  ~PrintLayout();
  int page_count() const;
  QSizeF page_size() const;
  QString text() const;
  void paint_page(QPainter& painter, int page) const;
 private:
  struct Data;
  std::unique_ptr<Data> data_;
};

void configure_printer_for_jwp(QPrinter& printer,
                               const core::JwpDocument& document);
void print_document(QPrinter& printer, const QTextDocument& source,
                    const core::JwpDocument* jwp_document = nullptr,
                    PrintOptions options = {});

}  // namespace jwpqt::qt

#endif
