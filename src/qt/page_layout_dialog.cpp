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
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "jwpqt/core/jwp_text_codec.h"
#include "help_window.h"
#include "kana_input_field.h"
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
constexpr double kCentimetersPerInch = 2.54;

double displayed_margin(double inches, bool metric) {
  return metric ? inches * kCentimetersPerInch : inches;
}

double stored_margin(double value, bool metric) {
  return metric ? value / kCentimetersPerInch : value;
}

QDoubleSpinBox* margin_spin(const char* name, QWidget* parent, bool metric) {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setObjectName(QString::fromLatin1(name));
  spin->setRange(0.0, metric ? 10.0 * kCentimetersPerInch : 10.0);
  spin->setDecimals(8);
  spin->setSingleStep(0.1);
  spin->setSuffix(metric ? PageLayoutDialog::tr(" cm")
                         : PageLayoutDialog::tr(" in"));
  return spin;
}

}  // namespace

PageLayoutDialog::PageLayoutDialog(const core::JwpDocument& document,
                                   core::LegacyCodePage code_page,
                                   QWidget* parent, const core::JwpPageDefaults* defaults,
                                   bool metric_units,
                                   QAction* overwrite_action)
    : QDialog(parent),
      document_(document),
      code_page_(code_page),
      metric_units_(metric_units),
      margins_{margin_spin(kMarginNames[0], this, metric_units),
               margin_spin(kMarginNames[1], this, metric_units),
               margin_spin(kMarginNames[2], this, metric_units),
               margin_spin(kMarginNames[3], this, metric_units)},
      landscape_(new QCheckBox(tr("&Landscape"), this)),
      vertical_(new QCheckBox(tr("&Vertical printing"), this)),
      separate_headers_(new QCheckBox(tr("Separate odd and even"), this)),
      suppress_first_(new QCheckBox(tr("Suppress on first page"), this)),
      status_(new QLabel(this)) {
  if (defaults) { (void)core::encode_page_defaults(*defaults); defaults_ = *defaults; }
  setObjectName(QStringLiteral("pageLayoutDialog"));
  setWindowTitle(tr("Page Layout"));
  resize(620, 520);

  for (const float margin : document_.margins) {
    if (!std::isfinite(margin) || margin < 0.0F || margin > 10.0F)
      throw core::JwpFormatError("Page margin is outside the supported range");
  }

  auto* outer = new QVBoxLayout(this);
  auto* tabs = new QTabWidget(this);
  tabs->setObjectName(QStringLiteral("pageLayoutTabs"));
  auto* margins_page = new QWidget(tabs);
  auto* margins_layout = new QFormLayout(margins_page);
  const std::array<QString, 4> margin_labels{tr("Left"), tr("Right"),
                                             tr("Top"), tr("Bottom")};
  for (std::size_t index = 0; index < margins_.size(); ++index) {
    margins_[index]->setValue(
        displayed_margin(document_.margins[index], metric_units_));
    connect(margins_[index], &QDoubleSpinBox::valueChanged, this, [this, index] { margins_changed_[index] = true; });
    margins_layout->addRow(margin_labels[index], margins_[index]);
  }
  landscape_->setObjectName(QStringLiteral("layoutLandscape"));
  vertical_->setObjectName(QStringLiteral("layoutVertical"));
  landscape_->setChecked(document_.landscape);
  vertical_->setChecked(document_.vertical);
  margins_layout->addRow(landscape_);
  margins_layout->addRow(vertical_);
  if (defaults_) {
    auto* load = new QPushButton(tr("From Default"), margins_page);
    auto* save = new QPushButton(tr("Set as Default"), margins_page);
    load->setObjectName(QStringLiteral("layoutFromDefault"));
    save->setObjectName(QStringLiteral("layoutSetDefault"));
    margins_layout->addRow(load, save);
    connect(load, &QPushButton::clicked, this, [this] {
      document_.margins = defaults_->margins;
      for (std::size_t i = 0; i < 4; ++i) {
        margins_[i]->setValue(
            displayed_margin(document_.margins[i], metric_units_));
        margins_changed_[i] = false;
      }
      landscape_->setChecked(defaults_->landscape); vertical_->setChecked(defaults_->vertical);
    });
    connect(save, &QPushButton::clicked, this, [this] {
      for (std::size_t i = 0; i < 4; ++i)
        defaults_->margins[i] = margins_changed_[i]
                                    ? static_cast<float>(stored_margin(
                                          margins_[i]->value(), metric_units_))
                                    : document_.margins[i];
      defaults_->landscape = landscape_->isChecked(); defaults_->vertical = vertical_->isChecked();
      status_->setText(tr("New defaults are staged. Accept this dialog to keep them."));
    });
  }
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
      auto* field = new KanaInputField(
          QStringLiteral("layoutHeader%1_%2").arg(set).arg(position), page);
      field->set_overwrite_action(overwrite_action);
      field->edit()->setMaxLength(65'535);
      field->edit()->setText(to_qstring(core::decode_jwp_text(
          document_.headers[set][position], code_page_)));
      headers_[set][position] = field;
      form->addRow(tr(kHeaderPositions[position]), field);
    }
    header_tabs->addTab(page, tr(kHeaderSets[set]));
  }
  headers_layout->addWidget(header_tabs);
  tabs->addTab(headers_page, tr("Headers and footers"));

  auto* summary_page = new QWidget(tabs);
  auto* summary_layout = new QFormLayout(summary_page);
  for (std::size_t index = 0; index < summary_.size(); ++index) {
    auto* field = new KanaInputField(
        QString::fromLatin1(kSummaryNames[index]), summary_page);
    field->set_overwrite_action(overwrite_action);
    field->edit()->setMaxLength(65'535);
    field->edit()->setText(
        to_qstring(core::decode_jwp_text(document_.summary[index], code_page_)));
    summary_[index] = field;
    summary_layout->addRow(tr(kSummaryLabels[index]), field);
  }
  tabs->addTab(summary_page, tr("Summary"));
  outer->addWidget(tabs, 1);

  status_->setObjectName(QStringLiteral("pageLayoutStatus"));
  outer->addWidget(status_);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help,
      this);
  auto* help = buttons->button(QDialogButtonBox::Help);
  help->setObjectName(QStringLiteral("pageLayoutHelp"));
  outer->addWidget(buttons);
  const auto help_topic = [tabs] {
    const QStringList topics = {
        QStringLiteral("IDH_PRINT_MARGINS"), QStringLiteral("IDH_PRINT_HEADERS"),
        QStringLiteral("IDH_PRINT_SUMMARY")};
    return topics.value(tabs->currentIndex(), QStringLiteral("IDH_PRINT_LAYOUT"));
  };
  setProperty("jwpqtHelpTopic", help_topic());
  connect(tabs, &QTabWidget::currentChanged, this,
          [this, help_topic](int) { setProperty("jwpqtHelpTopic", help_topic()); });
  connect(help, &QPushButton::clicked, this, [this, help_topic] {
    HelpWindow::open_owner_topic(this, help_topic());
  });
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
    for (auto& set : headers_)
      for (KanaInputField* field : set) field->finish_input();
    for (KanaInputField* field : summary_) field->finish_input();
    core::JwpDocument candidate = document_;
    for (std::size_t index = 0; index < candidate.margins.size(); ++index)
      if (margins_changed_[index])
        candidate.margins[index] = static_cast<float>(
            stored_margin(margins_[index]->value(), metric_units_));
    candidate.landscape = landscape_->isChecked();
    candidate.vertical = vertical_->isChecked();
    candidate.separate_left_right_headers = separate_headers_->isChecked();
    candidate.suppress_first_page_headers = suppress_first_->isChecked();
    for (std::size_t set = 0; set < candidate.headers.size(); ++set) {
      for (std::size_t position = 0; position < candidate.headers[set].size();
           ++position) {
        candidate.headers[set][position] = core::encode_jwp_text(
            from_qstring(headers_[set][position]->edit()->text()), code_page_);
      }
    }
    for (std::size_t index = 0; index < candidate.summary.size(); ++index) {
      candidate.summary[index] = core::encode_jwp_text(
          from_qstring(summary_[index]->edit()->text()), code_page_);
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
