// SPDX-License-Identifier: GPL-2.0-or-later

#include "page_layout_dialog.h"

#include <cmath>
#include <exception>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "text_bridge.h"

namespace jwpqt::qt {
namespace {

constexpr std::array<const char*, 4> kMarginNames{
    "leftMargin", "rightMargin", "topMargin", "bottomMargin"};
constexpr std::array<const char*, 5> kSummaryNames{
    "layoutTitle", "layoutSubject", "layoutAuthor", "layoutKeywords",
    "layoutNotes"};
constexpr std::array<const char*, 5> kSummaryLabels{
    "Title", "Subject", "Author", "Keywords", "Notes"};
constexpr std::array<const char*, 4> kHeaderSets{
    "Odd header", "Even header", "Odd footer", "Even footer"};
constexpr std::array<const char*, 3> kHeaderPositions{"Left", "Center",
                                                      "Right"};

QDoubleSpinBox* margin_spin(const char* name, QWidget* parent) {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setObjectName(QString::fromLatin1(name));
  spin->setRange(0.0, 10.0);
  spin->setDecimals(2);
  spin->setSingleStep(0.1);
  spin->setSuffix(PageLayoutDialog::tr(" in"));
  return spin;
}

}  // namespace

PageLayoutDialog::PageLayoutDialog(const core::JwpDocument& document,
                                   core::LegacyCodePage code_page,
                                   QWidget* parent)
    : QDialog(parent),
      document_(document),
      code_page_(code_page),
      margins_{margin_spin(kMarginNames[0], this),
               margin_spin(kMarginNames[1], this),
               margin_spin(kMarginNames[2], this),
               margin_spin(kMarginNames[3], this)},
      landscape_(new QCheckBox(tr("&Landscape"), this)),
      vertical_(new QCheckBox(tr("&Vertical printing"), this)),
      separate_headers_(new QCheckBox(tr("Separate odd and even"), this)),
      suppress_first_(new QCheckBox(tr("Suppress on first page"), this)),
      status_(new QLabel(this)) {
  setObjectName(QStringLiteral("pageLayoutDialog"));
  setWindowTitle(tr("Page Layout"));
  resize(620, 520);

  for (const float margin : document_.margins) {
    if (!std::isfinite(margin) || margin < 0.0F || margin > 10.0F)
      throw core::JwpFormatError("Page margin is outside the supported range");
  }

  auto* outer = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  auto* margins_page = new QWidget(tabs);
  auto* margins_layout = new QFormLayout(margins_page);
  const std::array<QString, 4> margin_labels{tr("Left"), tr("Right"),
                                             tr("Top"), tr("Bottom")};
  for (std::size_t index = 0; index < margins_.size(); ++index) {
    margins_[index]->setValue(document_.margins[index]);
    margins_layout->addRow(margin_labels[index], margins_[index]);
  }
  landscape_->setObjectName(QStringLiteral("layoutLandscape"));
  vertical_->setObjectName(QStringLiteral("layoutVertical"));
  landscape_->setChecked(document_.landscape);
  vertical_->setChecked(document_.vertical);
  margins_layout->addRow(landscape_);
  margins_layout->addRow(vertical_);
  tabs->addTab(margins_page, tr("Margins"));

  auto* headers_page = new QWidget(tabs);
  auto* headers_layout = new QVBoxLayout(headers_page);
  separate_headers_->setObjectName(QStringLiteral("layoutSeparateHeaders"));
  suppress_first_->setObjectName(QStringLiteral("layoutSuppressFirst"));
  separate_headers_->setChecked(document_.separate_left_right_headers);
  suppress_first_->setChecked(document_.suppress_first_page_headers);
  headers_layout->addWidget(separate_headers_);
  headers_layout->addWidget(suppress_first_);
  auto* header_tabs = new QTabWidget(headers_page);
  for (std::size_t set = 0; set < headers_.size(); ++set) {
    auto* page = new QWidget(header_tabs);
    auto* form = new QFormLayout(page);
    for (std::size_t position = 0; position < headers_[set].size(); ++position) {
      auto* edit = new QLineEdit(page);
      edit->setObjectName(
          QStringLiteral("layoutHeader%1_%2").arg(set).arg(position));
      edit->setMaxLength(65'535);
      edit->setText(to_qstring(core::decode_jwp_text(
          document_.headers[set][position], code_page_)));
      headers_[set][position] = edit;
      form->addRow(tr(kHeaderPositions[position]), edit);
    }
    header_tabs->addTab(page, tr(kHeaderSets[set]));
  }
  headers_layout->addWidget(header_tabs);
  tabs->addTab(headers_page, tr("Headers and footers"));

  auto* summary_page = new QWidget(tabs);
  auto* summary_layout = new QFormLayout(summary_page);
  for (std::size_t index = 0; index < summary_.size(); ++index) {
    auto* edit = new QLineEdit(summary_page);
    edit->setObjectName(QString::fromLatin1(kSummaryNames[index]));
    edit->setMaxLength(65'535);
    edit->setText(
        to_qstring(core::decode_jwp_text(document_.summary[index], code_page_)));
    summary_[index] = edit;
    summary_layout->addRow(tr(kSummaryLabels[index]), edit);
  }
  tabs->addTab(summary_page, tr("Summary"));
  outer->addWidget(tabs, 1);

  status_->setObjectName(QStringLiteral("pageLayoutStatus"));
  outer->addWidget(status_);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  outer->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    if (apply_changes()) accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

const core::JwpDocument& PageLayoutDialog::document() const noexcept {
  return document_;
}

bool PageLayoutDialog::apply_changes() {
  try {
    core::JwpDocument candidate = document_;
    for (std::size_t index = 0; index < candidate.margins.size(); ++index)
      candidate.margins[index] = static_cast<float>(margins_[index]->value());
    candidate.landscape = landscape_->isChecked();
    candidate.vertical = vertical_->isChecked();
    candidate.separate_left_right_headers = separate_headers_->isChecked();
    candidate.suppress_first_page_headers = suppress_first_->isChecked();
    for (std::size_t set = 0; set < candidate.headers.size(); ++set) {
      for (std::size_t position = 0; position < candidate.headers[set].size();
           ++position) {
        candidate.headers[set][position] = core::encode_jwp_text(
            from_qstring(headers_[set][position]->text()), code_page_);
      }
    }
    for (std::size_t index = 0; index < candidate.summary.size(); ++index) {
      candidate.summary[index] = core::encode_jwp_text(
          from_qstring(summary_[index]->text()), code_page_);
    }
    document_ = std::move(candidate);
    status_->clear();
    return true;
  } catch (const std::exception& error) {
    status_->setText(QString::fromUtf8(error.what()));
  } catch (...) {
    status_->setText(tr("Page layout could not be applied"));
  }
  return false;
}

}  // namespace jwpqt::qt
