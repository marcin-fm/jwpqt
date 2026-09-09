// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <limits>

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

#include "jwpqt/core/jwp_text_codec.h"
#include "page_layout_dialog.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

void type_key(QLineEdit& edit, int key, const QString& text) {
  QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
  QApplication::sendEvent(&edit, &press);
  QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
  QApplication::sendEvent(&edit, &release);
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

void test_japanese_metadata_input() {
  auto source = document();
  source.summary[0] = jwpqt::core::encode_jwp_text(U"かき");
  source.headers[0][0] = jwpqt::core::encode_jwp_text(U"見出し");
  QAction overwrite(nullptr);
  overwrite.setCheckable(true);
  overwrite.setChecked(true);
  jwpqt::qt::PageLayoutDialog dialog(
      source, jwpqt::core::LegacyCodePage::k1252, nullptr, nullptr, false,
      &overwrite);
  auto* title =
      dialog.findChild<QLineEdit*>(QStringLiteral("layoutTitle"));
  auto* header =
      dialog.findChild<QLineEdit*>(QStringLiteral("layoutHeader0_0"));
  auto* title_mode =
      dialog.findChild<QToolButton*>(QStringLiteral("layoutTitleMode"));
  require(title != nullptr && header != nullptr && title_mode != nullptr &&
              title_mode->text() == QStringLiteral("K") &&
              title->toolTip().contains(QStringLiteral("Overwrite")),
          "Page-layout text fields did not expose shared Japanese input");

  title->setCursorPosition(0);
  type_key(*title, Qt::Key_N, QStringLiteral("n"));
  type_key(*title, Qt::Key_A, QStringLiteral("a"));
  header->setCursorPosition(header->text().size());
  type_key(*header, Qt::Key_N, QStringLiteral("n"));
  require(dialog.apply_changes() &&
              jwpqt::core::decode_jwp_text(dialog.document().summary[0]) ==
                  U"なき" &&
              jwpqt::core::decode_jwp_text(dialog.document().headers[0][0]) ==
                  U"見出しん",
          "Page-layout text did not apply overwrite or pending Japanese input");
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
  test_japanese_metadata_input();
  test_invalid_source_margin_is_rejected();
  return EXIT_SUCCESS;
}
