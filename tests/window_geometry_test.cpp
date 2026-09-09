// SPDX-License-Identifier: GPL-2.0-or-later
#include "application_settings.h"
#include "main_window.h"
#include "application_settings_dialog.h"
#include "jwp_editor.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QScreen>
#include <QTemporaryDir>
#include <QPointer>
#include <QDynamicPropertyChangeEvent>
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace jwpqt::qt;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

class DeleteOnPlacement final : public QObject {
 public:
  QPointer<MainWindow> owner;
  bool eventFilter(QObject*, QEvent* event) override {
    if (event->type() == QEvent::DynamicPropertyChange &&
        static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName() == "_jwpqt_geometry_initialized") {
      delete owner.data();
      return true;
    }
    return false;
  }
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  try {
    auto settings = read_application_settings("usedims=true\nmaximize=false\nx=-2147483648\n"
        "Window.Y=31\nxs=530\nys=420\nsize_info.x=41\nCharInfo.Y=51\nCharInfo.W=340\nCharInfo.H=220\nUnknown=opaque\n");
    require(settings.restore_window && settings.window_geometry[0][0] == std::numeric_limits<int>::min(), "Sentinel/alias lost");
    const auto encoded = write_application_settings(settings);
    require(read_application_settings(encoded).window_geometry == settings.window_geometry &&
        encoded.find("Unknown=opaque") != std::string::npos, "Geometry roundtrip lost data");
    for (const auto* bad : {"Window.W=-1\n", "Window.X=1000001\n", "CharInfo.H=32769\n",
             "Window.Y=invalid\nWindow.Y=1\n", "RestoreWindow=invalid\nusedims=true\n"}) {
      bool failed = false;
      try { (void)read_application_settings(bad); } catch (const std::exception&) { failed = true; }
      require(failed, "Invalid geometry accepted");
    }
    QTemporaryDir temporary;
    require(temporary.isValid(), "No temporary directory");
    MainWindow window;
    ApplicationSettingsDialog options(settings);
    auto* restore = options.findChild<QCheckBox*>("settingsRestoreWindow");
    require(restore && restore->isChecked(), "Restore option missing");
    restore->click(); options.reject();
    require(options.settings().restore_window, "Cancelled option changed settings");
    require(window.apply_application_settings(settings), "Settings rejected");
    const QString original = window.active_editor()->document()->toRawText();
    const auto before = window.application_settings().window_geometry;
    require(before == settings.window_geometry, "Hidden window changed stored placement");
    window.show(); app.processEvents();
    require(window.size() == QSize(530, 420).boundedTo(window.screen()->availableGeometry().size())
        .expandedTo(window.minimumSize()), "Main size not restored");
    require(window.screen()->availableGeometry().contains(window.geometry().topLeft()), "Main restored offscreen");
    auto changed = window.application_settings();
    changed.window_geometry[0] = {999999, -999999, 600, 450};
    const QRect initial = window.geometry();
    require(window.apply_application_settings(changed), "Live settings rejected");
    require(window.geometry() == initial, "Live settings moved the window");
    window.move(37, 47); window.resize(550, 430); app.processEvents();
    const auto stored = window.application_settings().window_geometry[0];
    require(stored[2] == 550 && stored[3] == 430, "Normal bounds not captured");
    window.showMaximized(); app.processEvents();
    require(window.application_settings().maximize_window, "Maximized state not captured");
    require(window.application_settings().window_geometry[0][2] == 550, "Maximize replaced normal size");
    window.showMinimized(); app.processEvents();
    require(window.application_settings().maximize_window && window.application_settings().window_geometry[0][2] == 550,
        "Minimize destroyed restore state");
    const QString path = temporary.filePath("settings.cfg");
    require(window.save_application_settings(path), "Save failed");
    MainWindow restored;
    require(restored.load_application_settings(path), "Reload failed");
    restored.show(); app.processEvents();
    require(restored.isMaximized(), "Maximize not restored");
    restored.showNormal(); app.processEvents();
    require(restored.size() == QSize(550, 430).boundedTo(restored.screen()->availableGeometry().size())
        .expandedTo(restored.minimumSize()), "Normal size after maximize not restored");
    MainWindow other;
    other.show(); app.processEvents();
    const auto independent = other.application_settings().window_geometry;
    const QStringList names{"kanjiInfoDialog", "edictLookupDialog", "kanjiCountDialog", "kanjiInfoMoreDialog",
        "wnnUserDictionaryDialog", "edictUserDictionaryDialog"};
    for (int i = 0; i < names.size(); ++i) {
      QDialog dialog(&restored); dialog.setObjectName(names[i]); dialog.resize(350 + i, 240 + i);
      dialog.show(); app.processEvents();
      dialog.resize(380 + i, 260 + i); dialog.move(50 + i, 60 + i); app.processEvents();
      const auto saved = restored.application_settings().window_geometry[i + 1];
      dialog.hide();
      QDialog next(&restored); next.setObjectName(names[i]); next.show(); app.processEvents();
      require(next.size() == QSize(saved[2], saved[3]), "Dialog size not reused");
      require(other.application_settings().window_geometry == independent, "Other workspace captured a dialog");
    }
    auto offscreen = restored.application_settings(); offscreen.maximize_window = false;
    offscreen.window_geometry[0] = {999999, -999999, 32768, 32768};
    MainWindow clamped; require(clamped.apply_application_settings(offscreen), "Large bounded geometry rejected");
    clamped.show(); app.processEvents();
    require(clamped.screen()->availableGeometry().contains(clamped.geometry().topLeft()), "Missing monitor not clamped");
    auto disabled = settings; disabled.restore_window = false; disabled.maximize_window = true;
    MainWindow defaults; require(defaults.apply_application_settings(disabled), "Defaults rejected");
    const QSize default_size = defaults.size(); defaults.show(); app.processEvents();
    require(!defaults.isMaximized() && defaults.size() == default_size, "Disabled restore applied geometry");
    // Source-backed geometry passes through the actual project settings boundary.
    const QString project = temporary.filePath("geometry.jpr");
    require(restored.save_project_path(project, false), "Project save failed");
    MainWindow project_window;
    ProjectOpenOptions project_options;
    project_options.allow_unapplied_settings = true;
    require(project_window.open_project_path(project, project_options), "Project restore failed");
    require(project_window.application_settings().window_geometry == restored.application_settings().window_geometry,
        "Project lost geometry");
    require(window.active_editor()->document()->toRawText() == original &&
        !window.active_editor()->document()->isModified(), "Placement changed document state");
    DeleteOnPlacement deletion;
    deletion.owner = new MainWindow;
    QPointer<QDialog> doomed = new QDialog(deletion.owner);
    doomed->setObjectName("kanjiInfoDialog"); doomed->installEventFilter(&deletion);
    QEvent show(QEvent::Show); QApplication::sendEvent(doomed, &show);
    require(!deletion.owner && !doomed, "Owner deletion fixture did not run");
    app.sendPostedEvents(nullptr, QEvent::DeferredDelete);
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
  return 0;
}
