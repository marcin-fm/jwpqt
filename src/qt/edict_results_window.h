// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <vector>

#include <QWidget>
#include <QColor>

#include "edict_resource_search.h"

class QLabel;
class QListWidget;
class QPushButton;

namespace jwpqt::qt {

class EdictResultsWindow : public QWidget {
 public:
  using InsertHandler = std::function<bool(const std::u32string&)>;

  explicit EdictResultsWindow(QWidget* parent = nullptr);

  void set_insert_handler(InsertHandler handler);
  void set_highlight_color(const QColor& color);
  void append_report(EdictResourceSearchReport report);
  void clear_results();
  std::size_t result_count() const noexcept;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  struct StoredResult {
    std::u32string text;
    QString source;
  };

  void copy_selected();
  void insert_selected();
  std::vector<int> selected_rows() const;
  std::u32string selected_text() const;
  void update_status();
  void update_highlights();
  QColor highlight_color_;

  QListWidget* results_;
  QLabel* status_;
  QPushButton* copy_button_;
  QPushButton* insert_button_;
  std::vector<StoredResult> stored_results_;
  std::size_t rejected_ = 0;
  std::size_t failures_ = 0;
  InsertHandler insert_handler_;
};

}  // namespace jwpqt::qt
