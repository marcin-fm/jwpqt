// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include "find_replace_dialog.h"

class QAction;
class QAbstractButton;
class QWidget;
namespace jwpqt::qt {
class MainWindow;

// QTextEdit callers supply logical entry ranges in display order, excluding labels.
class AuxiliaryFind final : public QObject {
 public:
  using Ranges = std::function<std::vector<std::pair<int, int>>() >;
  explicit AuxiliaryFind(QWidget* target, Ranges ranges = {}, bool repeat_keys = true);
  void add_context_action(QAction* action);
  void set_result_insertion(QAbstractButton* insert_button);
  void open();
  FindReplaceResult find(const FindReplaceRequest& request);
 protected:
  bool eventFilter(QObject* object, QEvent* event) override;
 private:
  QPointer<QWidget> target_;
  QPointer<MainWindow> workspace_;
  QPointer<FindReplaceDialog> dialog_;
  std::shared_ptr<core::QueryHistories> histories_;
  Ranges ranges_;
  FindReplaceRequest last_;
  bool busy_ = false;
  bool repeat_keys_;
  QAction *open_, *next_, *previous_;
  QPointer<QAbstractButton> insert_button_;
  std::vector<QPointer<QAction>> context_actions_;
  std::vector<QAction*> insert_actions_;
  void update_insert_actions();
};
}
