// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_PAGE_LAYOUT_DIALOG_H
#define JWPQT_QT_PAGE_LAYOUT_DIALOG_H

#include <array>
#include <optional>

#include <QDialog>

#include "jwpqt/core/jwp_document.h"
#include "jwpqt/core/print_format.h"
#include "jwpqt/core/legacy_code_page.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

namespace jwpqt::qt {

class PageLayoutDialog : public QDialog {
 public:
  PageLayoutDialog(const core::JwpDocument& document,
                   core::LegacyCodePage code_page, QWidget* parent = nullptr,
                   const core::JwpPageDefaults* defaults = nullptr);

  const core::JwpDocument& document() const noexcept;
  bool apply_changes();
  const std::optional<core::JwpPageDefaults>& default_page() const noexcept { return defaults_; }

 private:
  core::JwpDocument document_;
  core::LegacyCodePage code_page_;
  std::optional<core::JwpPageDefaults> defaults_;
  std::array<bool, 4> margins_changed_{};
  std::array<QDoubleSpinBox*, 4> margins_;
  QCheckBox* landscape_;
  QCheckBox* vertical_;
  QCheckBox* separate_headers_;
  QCheckBox* suppress_first_;
  std::array<std::array<QLineEdit*, 3>, 4> headers_;
  std::array<QLineEdit*, 5> summary_;
  QLabel* status_;
};

}  // namespace jwpqt::qt

#endif
