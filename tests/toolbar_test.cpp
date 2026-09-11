// SPDX-License-Identifier: GPL-2.0-or-later
#include "main_window.h"
#include "toolbar_dialog.h"
#include "project_workspace.h"
#include "jwp_editor.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace jwpqt;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T* child(QObject& parent, const char* name) {
  auto* result = parent.findChild<T*>(QString::fromLatin1(name));
  require(result, name); return result;
}
template<class F> void reject(F function) {
  bool failed = false;
  try { function(); } catch (const std::exception&) { failed = true; }
  require(failed, "Invalid toolbar settings were accepted");
}
int icon_ink(const QIcon& icon, QIcon::Mode mode) {
  const QImage image = icon.pixmap(QSize(32, 32), 1.0, mode).toImage();
  require(!image.isNull() && image.pixelColor(0, 0).alpha() == 0,
          "Toolbar SVG did not render on a transparent background");
  std::uint64_t weighted = 0;
  std::uint64_t alpha = 0;
  for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
    const QColor pixel = image.pixelColor(x, y);
    weighted += static_cast<std::uint64_t>(qGray(pixel.rgb())) * pixel.alpha();
    alpha += pixel.alpha();
  }
  require(alpha > 255 * 12, "Toolbar SVG has no visible artwork");
  return static_cast<int>(weighted / alpha);
}
void settings() {
  qt::ApplicationSettings config;
  const std::array<std::uint8_t, 39> source_default{{
      0,1,2,3,0,5,0,11,12,13,0,6,7,0,14,15,16,0,8,9,10,18,0,19,28,34,29,0,
      20,21,22,23,24,25,26,27,0,33,36}};
  require(config.toolbar.count == static_cast<int>(source_default.size()) &&
              std::equal(source_default.begin(), source_default.end(),
                         config.toolbar.buttons.begin()),
          "Native toolbar did not use the recovered source default");
  const auto encoded = qt::write_application_settings(config);
  require(qt::write_application_settings(qt::read_application_settings(encoded)) == encoded, "Default toolbar drifted");
  for (int id = 0; id <= 36; ++id) {
    config.toolbar.count = 1; config.toolbar.buttons[0] = id; config.toolbar.buttons[99] = 255;
    auto restored = qt::read_application_settings(qt::write_application_settings(config));
    require(restored.toolbar.buttons == config.toolbar.buttons, "Toolbar ID or inactive tail lost");
    qt::ProjectWorkspace workspace; workspace.settings = config; workspace.detect_formats = false;
    const auto project = qt::encode_project_workspace(workspace);
    require(qt::read_application_settings(project.configuration).toolbar.buttons == config.toolbar.buttons,
            "Project lost toolbar settings");
  }
  auto reset = qt::read_application_settings("button_count=0\n", config);
  require(reset.toolbar.count == 0 && reset.toolbar.buttons[99] == 255, "Count-zero reset lost inactive data");
  std::string bytes(200, '0'); bytes.replace(0, 2, "FF");
  reject([&] { qt::read_application_settings("button_count=1\nbuttons=" + bytes); });
  require(qt::read_application_settings("button_count=0\nbuttons=" + bytes).toolbar.buttons[0] == 255,
          "Default sentinel did not preserve inactive bytes");
  for (const auto* bad : {"button_count=-1", "ToolbarButtonCount=101", "Jwpqt_ToolbarArea=4",
       "Jwpqt_ToolbarTextStyle=-1", "Jwpqt_ToolbarIconSize=15", "Jwpqt_ToolbarLocked=bad",
       "button_count=101\nbutton_count=1", "ToolbarButtons=00"}) reject([&] { qt::read_application_settings(bad); });
  config.toolbar.buttons[0] = 37;
  reject([&] { qt::write_application_settings(config); });
  config.toolbar.count = 100;
  reject([&] { qt::write_application_settings(config); });
  config = {};
  config.toolbar.count = 100;
  for (int i = 0; i < 100; ++i) config.toolbar.buttons[i] = i % 37;
  require(qt::read_application_settings(qt::write_application_settings(config)).toolbar.count == 100,
          "Maximum toolbar capacity rejected");
  for (int i = 0; i < 4; ++i) {
    config.toolbar.area = i; config.toolbar.text_style = i; config.toolbar.icon_size = 48; config.toolbar.locked = true;
    const auto copy = qt::read_application_settings(qt::write_application_settings(config));
    require(copy.toolbar.area == i && copy.toolbar.text_style == i && copy.toolbar.icon_size == 48 && copy.toolbar.locked,
            "Native toolbar preferences did not round-trip");
  }
}
void window() {
  const QPalette original_palette = QApplication::palette();
  QPalette light = original_palette;
  light.setColor(QPalette::Window, QColor(246, 246, 246));
  light.setColor(QPalette::Base, Qt::white);
  light.setColor(QPalette::Button, QColor(246, 246, 246));
  light.setColor(QPalette::Text, QColor(16, 18, 20));
  light.setColor(QPalette::ButtonText, QColor(16, 18, 20));
  light.setColor(QPalette::WindowText, QColor(16, 18, 20));
  light.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(126, 128, 130));
  QApplication::setPalette(light);
  QTemporaryDir directory(QDir::currentPath() + "/toolbar-XXXXXX"); require(directory.isValid(), "Temporary directory");
  qt::MainWindow window; window.resize(1500, 700); window.show(); QApplication::processEvents();
  auto* bar = child<QToolBar>(window, "mainToolBar");
  const auto initial = bar->actions();
  require(initial.size() == 39 && initial.front()->isSeparator() &&
              initial.back() == child<QAction>(window, "applicationOptionsAction"),
          "Native default omitted the source separator or Options command");
  auto* editor = window.active_editor(); require(editor, "Editor missing");
  editor->insertPlainText(QStringLiteral("before"));
  auto text = editor->toPlainText();
  int undo = editor->document()->availableUndoSteps();
  bool modified = editor->document()->isModified();
  auto config = window.application_settings();
  config.toolbar.buttons[99] = 255;
  config.toolbar.count = 36;
  for (int i = 0; i < 36; ++i) config.toolbar.buttons[i] = i + 1;
  require(window.apply_application_settings(config), "Full catalog apply failed");
  for (int id = 1; id <= 36; ++id)
    require(bar->actions()[id - 1] == child<QAction>(window, qt::kToolbarCommands[id]), "Toolbar did not reuse menu command");
  require(QFile::exists(QStringLiteral(":/jwpqt/assets/icons/new.svg")) &&
              QFile::exists(QStringLiteral(":/jwpqt/assets/icons/jwpqt.svg")) &&
              !QFile::exists(QStringLiteral(":/jwpqt/toolbar.bmp")) &&
              !QFile::exists(QStringLiteral(":/jwpqt/mainicon.ico")),
          "Toolbar retained a raster runtime resource");
  std::set<qint64> light_keys;
  for (int id = 1; id <= 36; ++id) {
    const QIcon icon = bar->actions()[id - 1]->icon();
    require(!icon.isNull() && icon_ink(icon, QIcon::Normal) < 64 &&
                icon_ink(icon, QIcon::Disabled) > 96,
            "Toolbar SVG did not use the light palette roles");
    light_keys.insert(icon.cacheKey());
  }
  require(light_keys.size() == 36, "Toolbar command SVGs are not independently addressable");
  require(window.grab().save(QStringLiteral("toolbar-vector-light.png")),
          "Could not save the light toolbar acceptance image");
  QPalette dark = light;
  dark.setColor(QPalette::Window, QColor(32, 35, 37));
  dark.setColor(QPalette::Base, QColor(24, 26, 28));
  dark.setColor(QPalette::Button, QColor(32, 35, 37));
  dark.setColor(QPalette::Text, QColor(240, 242, 244));
  dark.setColor(QPalette::ButtonText, QColor(240, 242, 244));
  dark.setColor(QPalette::WindowText, QColor(240, 242, 244));
  dark.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(140, 142, 144));
  QApplication::setPalette(dark);
  QApplication::processEvents();
  for (int id = 1; id <= 36; ++id) {
    const QIcon icon = bar->actions()[id - 1]->icon();
    require(light_keys.count(icon.cacheKey()) == 0 &&
                icon_ink(icon, QIcon::Normal) > 200 &&
                icon_ink(icon, QIcon::Disabled) > 120 &&
                icon_ink(icon, QIcon::Disabled) < 170,
            "Toolbar SVG did not use the dark palette roles");
  }
  require(window.grab().save(QStringLiteral("toolbar-vector-dark.png")),
          "Could not save the dark toolbar acceptance image");
  QApplication::setPalette(light);
  QApplication::processEvents();
  config.toolbar.count = 4; config.toolbar.buttons[0] = 9; config.toolbar.buttons[1] = 0;
  config.toolbar.buttons[2] = 9; config.toolbar.buttons[3] = 4;
  config.toolbar.area = 2; config.toolbar.icon_size = 32; config.toolbar.text_style = 1; config.toolbar.locked = true;
  require(window.apply_application_settings(config), "Custom toolbar failed");
  require(window.toolBarArea(bar) == Qt::LeftToolBarArea && !bar->isMovable() && bar->iconSize().width() == 32 &&
          bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon, "Toolbar presentation incorrect");
  const QPointer<QAction> duplicate = bar->actions()[2];
  auto* ascii = child<QAction>(window, "asciiInputAction");
  duplicate->trigger(); require(ascii->isChecked() && duplicate->isChecked(), "Duplicate did not trigger shared command");
  ascii->setEnabled(false); require(!duplicate->isEnabled(), "Duplicate enabled state diverged"); ascii->setEnabled(true);
  duplicate->trigger();
  require(ascii->isChecked() && duplicate->isChecked(), "Repeated exclusive command lost its checked state");
  require(bar->actions()[3] == child<QAction>(window, "deleteDocumentAction"), "Optional delete command missing");
  require(window.new_document_tab(false) >= 0,
          "Could not create unrestricted Unicode toolbar fixture");
  editor = window.active_editor(); editor->insertPlainText("unicode");
  text = editor->toPlainText(); undo = editor->document()->availableUndoSteps(); modified = editor->document()->isModified();
  const auto preserved = qt::write_application_settings(window.application_settings());
  const auto preserved_toolbar = window.application_settings().toolbar;
  const auto toolbar_matches = [&] {
    const auto& toolbar = window.application_settings().toolbar;
    return toolbar.buttons == preserved_toolbar.buttons &&
           toolbar.count == preserved_toolbar.count &&
           toolbar.area == preserved_toolbar.area &&
           toolbar.icon_size == preserved_toolbar.icon_size &&
           toolbar.text_style == preserved_toolbar.text_style &&
           toolbar.locked == preserved_toolbar.locked;
  };
  auto invalid = config; invalid.toolbar.buttons[0] = 255;
  require(!window.apply_application_settings(invalid) && qt::write_application_settings(window.application_settings()) == preserved,
          "Invalid apply damaged toolbar");
  QTimer::singleShot(0, [&] {
    auto* dialog = child<QDialog>(window, "toolbarDialog");
    auto* selected = child<QListWidget>(*dialog, "toolbarSelected"); selected->setCurrentRow(0);
    child<QPushButton>(*dialog, "toolbarRemove")->click();
    child<QDialogButtonBox>(*dialog, "")->button(QDialogButtonBox::Cancel)->click();
  });
  child<QAction>(window, "customizeToolbarAction")->trigger();
  require(toolbar_matches() && duplicate,
           "Cancel changed live toolbar");
  QTimer::singleShot(0, [&] {
    auto* dialog = child<QDialog>(window, "toolbarDialog");
    auto* selected = child<QListWidget>(*dialog, "toolbarSelected");
    selected->setCurrentRow(0); child<QPushButton>(*dialog, "toolbarDown")->click();
    child<QPushButton>(*dialog, "toolbarUp")->click();
    child<QPushButton>(*dialog, "toolbarRemove")->click();
    auto* available = child<QListWidget>(*dialog, "toolbarAvailable"); available->setCurrentRow(36);
    child<QPushButton>(*dialog, "toolbarAdd")->click();
    child<QComboBox>(*dialog, "toolbarArea")->setCurrentIndex(1);
    child<QSpinBox>(*dialog, "toolbarIconSize")->setValue(24);
    child<QComboBox>(*dialog, "toolbarTextStyle")->setCurrentIndex(2);
    child<QCheckBox>(*dialog, "toolbarLocked")->setChecked(false);
    dialog->grab().save("toolbar-customization.png");
    const auto light = QApplication::palette();
    auto dark = light;
    dark.setColor(QPalette::Window, QColor(32,35,37)); dark.setColor(QPalette::Base, QColor(32,35,37));
    dark.setColor(QPalette::Button, QColor(32,35,37)); dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::ButtonText, Qt::white); dark.setColor(QPalette::WindowText, Qt::white);
    const auto key = available->item(8)->icon().cacheKey();
    QApplication::setPalette(dark); QApplication::processEvents();
    require(available->item(8)->icon().cacheKey() != key && selected->currentRow() >= 0,
            "Live palette update left stale customization icons");
    dialog->grab().save("toolbar-customization-dark.png");
    QApplication::setPalette(light); QApplication::processEvents();
    child<QDialogButtonBox>(*dialog, "")->button(QDialogButtonBox::Ok)->click();
  });
  child<QAction>(window, "customizeToolbarAction")->trigger();
  require(!duplicate && window.application_settings().toolbar.buttons[99] == 255 &&
          window.toolBarArea(bar) == Qt::BottomToolBarArea && bar->isMovable(), "Accepted customization incorrect");
  if (editor->toPlainText() != text || editor->document()->availableUndoSteps() != undo ||
      editor->document()->isModified() != modified)
    throw std::runtime_error("Toolbar state changed: text=" + text.toStdString() + " -> " + editor->toPlainText().toStdString() +
        ", undo=" + std::to_string(undo) + " -> " + std::to_string(editor->document()->availableUndoSteps()) +
        ", modified=" + std::to_string(modified) + " -> " + std::to_string(editor->document()->isModified()));
  const auto path = directory.path() + "/settings.cfg";
  const auto before_drag = window.application_settings();
  window.addToolBar(Qt::RightToolBarArea, bar);
  require(window.apply_application_settings(before_drag) && window.toolBarArea(bar) == Qt::BottomToolBarArea,
          "Restoring the previous settings ignored a dragged toolbar");
  window.addToolBar(Qt::RightToolBarArea, bar);
  require(window.save_application_settings(path), "Saving toolbar failed");
  qt::MainWindow restored; require(restored.load_application_settings(path), "Toolbar restore failed");
  require(restored.application_settings().toolbar.area == 3 &&
          restored.application_settings().toolbar.buttons == window.application_settings().toolbar.buttons &&
          restored.toolBarArea(child<QToolBar>(restored, "mainToolBar")) == Qt::RightToolBarArea, "Session toolbar lost");
  config = window.application_settings(); config.toolbar.count = 0;
  require(window.apply_application_settings(config) && bar->actions().size() == initial.size(), "Reset convention failed");
  QTimer::singleShot(0, [&] {
    auto* dialog = child<QDialog>(window, "toolbarDialog");
    auto* selected = child<QListWidget>(*dialog, "toolbarSelected");
    auto* add = child<QPushButton>(*dialog, "toolbarAdd");
    child<QListWidget>(*dialog, "toolbarAvailable")->setCurrentRow(0);
    for (int i = selected->count(); i < 100; ++i) add->click();
    require(selected->count() == 100 && !add->isEnabled(), "Toolbar exceeded its UI slot limit");
    auto* remove = child<QPushButton>(*dialog, "toolbarRemove");
    for (int i = selected->count(); i > 1; --i) { selected->setCurrentRow(0); remove->click(); }
    require(selected->count() == 1 && !remove->isEnabled(), "Empty toolbar did not preserve a slot");
    child<QPushButton>(*dialog, "toolbarReset")->click();
    require(child<QListWidget>(*dialog, "toolbarSelected")->count() == 39, "Reset Layout did not restore defaults");
    child<QDialogButtonBox>(*dialog, "")->button(QDialogButtonBox::Ok)->click();
  });
  child<QAction>(window, "customizeToolbarAction")->trigger();
  QTimer::singleShot(0, [&] {
    auto* confirmation = window.findChild<QMessageBox*>(); require(confirmation, "Defaults confirmation missing");
    confirmation->button(QMessageBox::Yes)->click();
  });
  child<QAction>(window, "defaultSettingsAction")->trigger();
  require(window.application_settings().toolbar.buttons[99] == 255 && bar->actions().size() == 39 &&
              bar->actions().back() == child<QAction>(window, "applicationOptionsAction"),
          "Defaults lost the inactive toolbar tail");
  qt::MainWindow native;
  const auto original_native = *native.current_jwp_document();
  native.active_editor()->insertPlainText("native");
  const auto edited_native = *native.current_jwp_document();
  auto native_settings = native.application_settings(); native_settings.toolbar = config.toolbar;
  native_settings.toolbar.area = 3;
  require(native.apply_application_settings(native_settings) && *native.current_jwp_document() == edited_native,
          "Toolbar changed native document metadata");
  child<QAction>(native, "undoAction")->trigger();
  require(*native.current_jwp_document() == original_native, "Toolbar lost native undo");
  native_settings.toolbar.count = 3; native_settings.toolbar.buttons[0] = 9;
  native_settings.toolbar.buttons[1] = 0; native_settings.toolbar.buttons[2] = 9;
  native_settings.toolbar.area = 2; native_settings.toolbar.locked = true;
  require(native.apply_application_settings(native_settings) &&
          native.save_as_path(directory.path() + "/native.jwp", std::nullopt) &&
          native.save_project_path(directory.path() + "/workspace.jpr", false), "Custom project save failed");
  require(restored.open_project_path(directory.path() + "/workspace.jpr"), "Custom project restore failed");
  auto* project_bar = child<QToolBar>(restored, "mainToolBar");
  require(restored.toolBarArea(project_bar) == Qt::LeftToolBarArea && !project_bar->isMovable() &&
          project_bar->actions().size() == 3 && project_bar->actions()[2]->property("toolbarProxy").toBool() &&
          restored.application_settings().toolbar.buttons == native_settings.toolbar.buttons,
          "Project failed to restore layout, tail, duplicates or placement");
  QPointer<qt::MainWindow> doomed = new qt::MainWindow;
  QTimer::singleShot(0, [&] { delete doomed.data(); });
  child<QAction>(*doomed, "customizeToolbarAction")->trigger();
  require(!doomed, "Modal owner deletion failed");
  QApplication::setPalette(original_palette);
}
int main(int argc, char** argv) {
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
  try { settings(); window(); std::cout << "Toolbar tests passed\n"; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
