// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include <QUrl>
class QLineEdit;
class QListWidget;
class QLabel;
class QTextBrowser;

namespace jwpqt::qt {
class HelpWindow final : public QDialog {
 public:
  explicit HelpWindow(QWidget* owner);
  ~HelpWindow() override;
  void open_topic(const QString& topic = QStringLiteral("start.md"));
  static void open_owner_topic(QWidget* origin, const QString& topic);
 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
 private:
  void navigate(const QUrl& url);
  void move_history(bool backward);
  bool navigating_ = false;
  QLineEdit* search_;
  QListWidget* topics_;
  QTextBrowser* browser_;
  QLabel* status_;
};
}
