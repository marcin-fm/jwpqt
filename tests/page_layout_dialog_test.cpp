// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <limits>

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>

#include "jwpqt/core/jwp_text_codec.h"
#include "page_layout_dialog.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

jwpqt::core::JwpDocument document() {
  jwpqt::core::JwpDocument value;
  value.margins = {1.0F, 1.25F, 1.5F, 1.75F};
  value.landscape = true;
  value.summary[0] = jwpqt::core::encode_jwp_text(U"Old title");
  value.headers[0][0] = jwpqt::core::encode_jwp_text(U"Old header");
  value.paragraphs = {jwpqt::core::JwpParagraph{{0x3021}, 125, -1, 2, 3,
                                                 false}};
  return value;
}

void test_candidate_application() {
  const auto source = document();
  jwpqt::qt::PageLayoutDialog dialog(
      source, jwpqt::core::LegacyCodePage::k1252);
  auto* left = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("leftMargin"));
  auto* landscape =
      dialog.findChild<QCheckBox*>(QStringLiteral("layoutLandscape"));
  auto* vertical =
      dialog.findChild<QCheckBox*>(QStringLiteral("layoutVertical"));
  auto* separate =
      dialog.findChild<QCheckBox*>(QStringLiteral("layoutSeparateHeaders"));
  auto* title =
      dialog.findChild<QLineEdit*>(QStringLiteral("layoutTitle"));
  auto* header =
      dialog.findChild<QLineEdit*>(QStringLiteral("layoutHeader0_0"));
  require(left != nullptr && landscape != nullptr && vertical != nullptr &&
              separate != nullptr && title != nullptr && header != nullptr &&
              left->value() == 1.0 && landscape->isChecked() &&
              title->text() == QStringLiteral("Old title") &&
              header->text() == QStringLiteral("Old header"),
          "Page-layout controls did not load document metadata");
  left->setValue(2.5);
  landscape->setChecked(false);
  vertical->setChecked(true);
  separate->setChecked(true);
  title->setText(QStringLiteral("New title"));
  header->setText(QStringLiteral("New header"));
  require(dialog.apply_changes(), "Valid page-layout changes were rejected");
  const auto& changed = dialog.document();
  require(changed.margins[0] == 2.5F && !changed.landscape && changed.vertical &&
              changed.separate_left_right_headers &&
              jwpqt::core::decode_jwp_text(changed.summary[0]) == U"New title" &&
              jwpqt::core::decode_jwp_text(changed.headers[0][0]) ==
                  U"New header" &&
              changed.paragraphs == source.paragraphs,
          "Page-layout application lost document state");
}

void test_failed_encoding_is_atomic() {
  const auto source = document();
  jwpqt::qt::PageLayoutDialog dialog(
      source, jwpqt::core::LegacyCodePage::k1252);
  dialog.findChild<QLineEdit*>(QStringLiteral("layoutTitle"))
      ->setText(QString::fromUtf8("\xF0\x9F\x98\x80"));
  require(!dialog.apply_changes() && dialog.document() == source &&
              !dialog.findChild<QLabel*>(QStringLiteral("pageLayoutStatus"))
                   ->text()
                   .isEmpty(),
          "Unrepresentable page-layout text partially changed the document");
}

void test_invalid_source_margin_is_rejected() {
  auto source = document();
  source.margins[0] = std::numeric_limits<float>::quiet_NaN();
  try {
    jwpqt::qt::PageLayoutDialog dialog(
        source, jwpqt::core::LegacyCodePage::k1252);
  } catch (const jwpqt::core::JwpFormatError&) {
    return;
  }
  require(false, "Invalid persisted page margin was silently normalized");
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  test_candidate_application();
  test_failed_encoding_is_atomic();
  test_invalid_source_margin_is_rejected();
  return EXIT_SUCCESS;
}
