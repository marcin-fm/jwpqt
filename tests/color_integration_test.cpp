// SPDX-License-Identifier: GPL-2.0-or-later
#include "application_settings.h"
#include "application_settings_dialog.h"
#include "main_window.h"
#include "jwp_editor.h"
#include "file_io.h"
#include "kanji_color_settings.h"
#include "rare_kanji_delegate.h"
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QDir>
#include <iostream>
#include <stdexcept>

using namespace jwpqt;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F call) {
  bool rejected = false; try { call(); } catch (const std::exception&) { rejected = true; }
  require(rejected, "Invalid source color accepted");
}

int main(int argc, char** argv) {
  QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
  try {
    require(qt::ApplicationSettings{}.deemphasize_rare_radicals,
            "Source default should deemphasize rarely used radicals");
    const auto imported = qt::read_application_settings("Color_Highlight=0000CC00\r\n"
        "colorkanji_color=0A141E00\nColor_RareKanji=FFFFFFFF\nColorKanji_Mode=1\n"
        "ColorizeRareKanji=true\nMarkRareKanjiInKanjiBars=true\nUnknown=opaque\n");
    require(imported.color_refs[0] == 0x00cc0000 && imported.color_refs[1] == 0x001e140a &&
        imported.color_refs[2] == 0xffffffff && imported.mark_rare_kanji, "COLORREF byte order changed");
    auto policy = qt::effective_kanji_color_policy(imported, {});
    require(policy.list_color == core::RgbColor{10,20,30} && policy.uncommon_color == core::RgbColor{0,250,0} &&
        policy.colorize_uncommon && policy.list_mode == core::KanjiListColorMode::kMatch, "Source color policy changed");
    const auto serialized = qt::write_application_settings(imported);
    require(qt::read_application_settings(serialized).color_refs == imported.color_refs &&
        serialized.find("Unknown=opaque") != std::string::npos, "Color roundtrip lost data");
    rejects([&] { qt::read_application_settings("Color_KanjiList=00\n" + serialized); });
    rejects([&] { qt::read_application_settings("ColorKanji_Mode=256\n" + serialized); });
    rejects([&] { qt::read_application_settings("colorize_rare=maybe\n" + serialized); });
    auto palette = qt::read_application_settings("Color_KanjiList=01000001\n");
    require(!palette.unapplied.empty() && qt::effective_kanji_color_policy(palette, policy).list_color == policy.list_color,
            "Palette reference silently became RGB");
    auto cleared = imported; cleared.color_refs[1].reset();
    require(!qt::read_application_settings(qt::write_application_settings(cleared)).color_refs[1], "Cleared override resurrected");

    QTemporaryDir dir; require(dir.isValid(), "Temporary directory failed");
    const auto ini = dir.filePath("colors.ini"), list_path = dir.filePath("colkanji.lst");
    core::KanjiColorPolicy old; old.list_mode = core::KanjiListColorMode::kMatch; old.list_color = {30,40,50};
    { QSettings store(ini, QSettings::IniFormat); qt::write_kanji_color_policy(store, old); }
    core::KanjiColorList list; list.add(0x3021); qt::write_kanji_color_list_file(list_path, list);
    qt::MainWindow window;
    require(window.load_kanji_color_configuration(ini, list_path, qt::OpenMode::kNonInteractive), "Native color load failed");
    require(window.kanji_color_policy().list_color == old.list_color && !window.application_settings().color_refs[1],
            "Absent defaults replaced native INI");
    core::JwpDocument doc; doc.paragraphs.resize(1); doc.paragraphs[0].text = {0x3021, 0x5021};
    const auto first = dir.filePath("first.jwp"), second = dir.filePath("second.jwp");
    qt::write_jwp_file(first, doc); qt::write_jwp_file(second, doc);
    require(window.open_jwp_path(first), "First document failed");
    auto* one = window.active_editor();
    require(window.open_jwp_path(second, core::kDefaultLegacyCodePage, qt::OpenMode::kNonInteractive, true), "Second document failed");
    auto* two = window.active_editor();
    two->moveCursor(QTextCursor::End); two->insertPlainText("A");
    require(window.apply_application_settings(imported), "Imported color application failed");
    for (auto* editor : {one, two}) {
      require(!editor->extraSelections().empty() && editor->extraSelections().first().format.foreground().color() == QColor(10,20,30),
              "Inactive or active editor retained stale color");
    }
    require(two->document()->isModified(), "Color application cleared dirty state");
    window.findChild<QAction*>("undoAction")->trigger();
    require(!two->document()->isModified(), "Color application broke native undo");
    require(window.load_kanji_color_configuration(ini, list_path, qt::OpenMode::kNonInteractive) &&
        window.kanji_color_policy().list_color == policy.list_color, "INI overrode explicit source settings");
    auto inherited = window.application_settings(); inherited.color_refs[1].reset();
    require(window.apply_application_settings(inherited) && window.kanji_color_policy().list_color == old.list_color,
            "Clearing an override did not immediately restore the native store");
    old.list_color = {12,34,56};
    require(window.set_kanji_color_policy(old, qt::OpenMode::kNonInteractive), "Native color change failed");
    require(qt::effective_kanji_color_policy(window.application_settings(), {}).list_color == old.list_color,
            "Native color action left stale config override");
    require(one->extraSelections().first().format.foreground().color() == QColor(12,34,56), "Color action ignored inactive tab");
    const auto cfg = dir.filePath("jwpqt.cfg");
    require(window.save_application_settings(cfg), "Settings save failed");
    qt::MainWindow restart;
    require(restart.load_application_settings(cfg) && restart.load_kanji_color_configuration(ini, list_path, qt::OpenMode::kNonInteractive) &&
        restart.kanji_color_policy().list_color == old.list_color && restart.application_settings().mark_rare_kanji,
        "Restart changed effective colors");
    const auto project = dir.filePath("colors.jpr");
    require(window.save_project_path(project, false), "Project save failed");
    qt::ProjectOpenOptions open; open.allow_unapplied_settings = true;
    require(restart.open_project_path(project, open) && restart.kanji_color_policy().list_color == old.list_color,
            "Project color migration failed");

    qt::ApplicationSettingsDialog cancel(imported);
    cancel.findChild<QLineEdit*>("settingsColor1")->setText("#112233"); cancel.reject();
    require(cancel.settings().color_refs[1] == imported.color_refs[1], "Cancelled color escaped");
    qt::ApplicationSettingsDialog options(imported);
    options.findChild<QLineEdit*>("settingsColor1")->setText("#112233");
    options.findChild<QCheckBox*>("settingsMarkRare")->setChecked(false);
    options.findChild<QPushButton*>("settingsColorInherit2")->click();
    auto* tabs = options.findChild<QTabWidget*>("applicationSettingsTabs");
    for (int i = 0; i < tabs->count(); ++i) if (tabs->tabText(i) == "Colors") tabs->setCurrentIndex(i);
    options.show(); QApplication::processEvents();
    require(options.grab().save(QDir::current().filePath("color-options.png")), "Color options capture failed");
    options.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    require(options.result() == QDialog::Accepted && options.settings().color_refs[1] == 0x00332211 &&
        !options.settings().mark_rare_kanji && !options.settings().color_refs[2], "Color options did not publish staged values");

    QWidget owner; QListWidget bar(&owner); bar.resize(200, 45);
    qt::install_rare_kanji_marks(&bar); bar.addItem(QString::fromUtf8("亜弌"));
    owner.show(); bar.show(); QApplication::processEvents();
    const auto text = bar.item(0)->text(); const auto plain = bar.grab().toImage();
    owner.setProperty("jwpqtMarkRareKanji", true); bar.doItemsLayout(); QApplication::processEvents();
    const auto marked = bar.grab().toImage();
    require(plain != marked && bar.item(0)->text() == text, "Rare marks changed canonical text or did not render");
    int changed = 0;
    for (int y = 0; y < plain.height(); ++y) for (int x = 0; x < plain.width(); ++x)
      changed += plain.pixel(x, y) != marked.pixel(x, y);
    require(changed >= 4 && changed <= 9 * bar.devicePixelRatioF() * bar.devicePixelRatioF(),
            "Marking moved text or changed more than the rare character's dot");
    bar.item(0)->setText(QString::fromUtf8("亜")); owner.setProperty("jwpqtMarkRareKanji", false); bar.doItemsLayout();
    QApplication::processEvents(); const auto common = bar.grab().toImage();
    owner.setProperty("jwpqtMarkRareKanji", true); bar.doItemsLayout(); QApplication::processEvents();
    require(common == bar.grab().toImage(), "A first-level character was marked or shifted");
    std::cout << "Source color integration passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
