// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "kanji_info_options.h"

namespace jwpqt::qt {

class KanjiInfoOptionsDialog : public QDialog {
 public:
  explicit KanjiInfoOptionsDialog(const KanjiInfoOptions& options, QWidget* parent = nullptr);
  const KanjiInfoOptions& options() const noexcept { return options_; }

 private:
  KanjiInfoOptions options_;
};

}  // namespace jwpqt::qt
