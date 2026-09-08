// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include <functional>
#include <memory>
#include "application_settings.h"
#include "jwpqt/core/jwp_search.h"
#include "jwpqt/core/query_history_file.h"

class QAction;
class QCheckBox;
class QLabel;
namespace jwpqt::qt {
class KanaInputField;
enum class SearchOperation { kFind, kReplace, kReview, kAll };
struct FindReplaceRequest {
  QString text;
  QString replacement;
  core::JwpSearchOptions options;
  bool all_files = false;
  bool keep_open = true;
  SearchOperation operation = SearchOperation::kFind;
};
struct FindReplaceResult { bool valid = false; QString message; };
class FindReplaceDialog : public QDialog {
 public:
  using Handler = std::function<FindReplaceResult(const FindReplaceRequest&)>;
  FindReplaceDialog(bool replacing, const ApplicationSettings& settings,
                    std::shared_ptr<core::QueryHistories> histories,
                    Handler handler, QWidget* parent = nullptr);
  void set_text(const QString& text, const QString& replacement);
  void set_overwrite_action(QAction* action);
 protected:
  bool eventFilter(QObject* object, QEvent* event) override;
 private:
  void submit(SearchOperation operation);
  void history(int field, bool chooser, int direction = 0);
  bool replacing_;
  bool busy_ = false;
  KanaInputField* fields_[2]{};
  QCheckBox *case_, *width_, *wrap_, *all_, *back_, *keep_;
  QLabel* status_;
  std::shared_ptr<core::QueryHistories> histories_;
  Handler handler_;
};
}
