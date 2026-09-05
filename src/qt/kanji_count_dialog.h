// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_QT_KANJI_COUNT_DIALOG_H
#define JWPQT_QT_KANJI_COUNT_DIALOG_H

#include <functional>
#include <string>
#include <vector>

#include <QDialog>

#include "jwpqt/core/kanji_count.h"
#include "jwpqt/core/kanji_info.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace jwpqt::qt {

struct KanjiCountDisplayOptions {
  bool all_documents = false;
  core::KanjiCountFilter filter = core::KanjiCountFilter::kAll;
  bool frequency = true;
  bool on_readings = false;
  bool kun_readings = false;
  bool meanings = false;
};

class KanjiCountDialog : public QDialog {
 public:
  using InsertHandler = std::function<void(std::u32string)>;
  using InfoHandler = std::function<void(core::JisCode)>;

  KanjiCountDialog(std::vector<const core::JwpDocument*> documents,
                   const core::KanjiColorList& color_list,
                   const core::KanjiInfoDatabase* information,
                   InsertHandler insert_handler, InfoHandler info_handler,
                   QWidget* parent = nullptr);

  void set_display_options(const KanjiCountDisplayOptions& options);
  bool count();
  std::vector<core::KanjiCountEntry> results() const;

 private:
  struct RenderedEntry {
    core::KanjiCountEntry entry;
    std::u32string text;
  };

  bool publish(core::KanjiCountReport report,
               const KanjiCountDisplayOptions& options);
  KanjiCountDisplayOptions display_options() const;
  std::vector<int> selected_rows() const;
  void update_actions();
  void copy_results();
  void insert_results();
  void show_information();

  std::vector<core::JwpDocument> documents_;
  const core::KanjiColorList& color_list_;
  const core::KanjiInfoDatabase* information_;
  InsertHandler insert_handler_;
  InfoHandler info_handler_;
  QCheckBox* all_documents_;
  QComboBox* filter_;
  QCheckBox* frequency_;
  QCheckBox* on_readings_;
  QCheckBox* kun_readings_;
  QCheckBox* meanings_;
  QListWidget* results_;
  QLabel* status_;
  QPushButton* copy_button_;
  QPushButton* insert_button_;
  QPushButton* info_button_;
  std::vector<RenderedEntry> rendered_;
};

}  // namespace jwpqt::qt

#endif
