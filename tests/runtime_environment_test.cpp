// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QStyleFactory>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include "main_window.h"
#include "jwp_editor.h"
#include "edict_lookup_dialog.h"
#include "kanji_info_dialog.h"
#include "kanji_code_lookup_dialog.h"
#include "kanji_lookup_dialog.h"
#include "kanji_reading_lookup_dialog.h"
#include "jis_table_dialog.h"
#include "edict_resources.h"
#include "jwpqt/core/edict_registry.h"
#include "jwpqt/core/edict_search.h"
#include "jwpqt/core/jwp_text_codec.h"
#include "jwpqt/core/kanji_bushu_selector.h"

namespace {

void require(bool condition, const QString& message) {
  if (!condition) {
    throw std::runtime_error(message.toStdString());
  }
}

void write_file(const QString& path, const QByteArray& bytes) {
  require(QDir().mkpath(QFileInfo(path).absolutePath()),
          QStringLiteral("Could not create fixture directory"));
  QFile file(path);
  require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(),
          QStringLiteral("Could not write fixture: ") + path);
}

void test_runtime_paths(const QString& executable, const QString& root) {
  const QString config = root + QStringLiteral("/app config");
  const QString personal = root + QStringLiteral("/personal data");
  const QString desktop = root + QStringLiteral("/desktop");
  const QByteArray settings("[kanjiColor]\npolicy=v1;invalid;010203;1;040506\n");
  const QString desktop_settings =
      desktop + QStringLiteral("/jwpqt/jwpqt/settings.ini");
  write_file(desktop_settings, settings);
  const QStringList options{QStringLiteral("--config-dir"), config,
                            QStringLiteral("--user-data-dir"), personal,
                            QStringLiteral("--resource-report")};
  auto run = [&](const QStringList& arguments, int expected) {
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                       QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), desktop);
    environment.insert(QStringLiteral("XDG_DATA_HOME"),
                       root + QStringLiteral("/desktop data"));
    process.setProcessEnvironment(environment);
    process.start(executable, arguments);
    require(process.waitForFinished(10000),
            QStringLiteral("Application did not finish: ") + process.errorString());
    const QString output = QString::fromUtf8(process.readAllStandardOutput()) +
                           QString::fromUtf8(process.readAllStandardError());
    require(process.exitStatus() == QProcess::NormalExit &&
                process.exitCode() == expected,
            QStringLiteral("Unexpected application exit: ") + output);
    return output;
  };

  const QString missing = run(options, 0);
  require(missing.contains(QStringLiteral("Configuration directory: ") + config) &&
              missing.contains(QStringLiteral("WNN conversion: unavailable")) &&
              missing.contains(QStringLiteral("Kanji information: unavailable")) &&
              missing.contains(QStringLiteral("Qt platform/style: offscreen")),
          QStringLiteral("Missing-resource report was not actionable: ") + missing);
  require(run({QStringLiteral("--smoke-test")}, 1)
              .contains(QStringLiteral("Could not load the kanji color configuration")),
          QStringLiteral("Explicit config did not bypass the default app settings"));
  const QString history = config + QStringLiteral("/recent-files.json");
  write_file(history, QByteArray("preserve corrupt history"));
  const QString history_report = run(options, 0);
  require(history_report.contains(QStringLiteral("Could not load recent files")),
          QStringLiteral("Invalid recent history blocked startup or was not disclosed"));
  QFile history_file(history);
  require(history_file.open(QIODevice::ReadOnly) &&
              history_file.readAll() == QByteArray("preserve corrupt history"),
          QStringLiteral("Startup overwrote corrupt recent history"));
  history_file.close();
  require(history_file.remove(), QStringLiteral("Could not remove history fixture"));
  const QString preferences = config + QStringLiteral("/jwpqt.cfg");
  const QByteArray invalid_preferences("File.Size=bad\nFile.Size=18\n");
  write_file(preferences, invalid_preferences);
  require(run(options, 0).contains(QStringLiteral("Could not load settings")),
          QStringLiteral("Invalid native settings blocked startup or were not disclosed"));
  QFile preferences_file(preferences);
  require(preferences_file.open(QIODevice::ReadOnly) && preferences_file.readAll() == invalid_preferences,
          QStringLiteral("Startup rewrote invalid native settings"));
  preferences_file.close();
  const QByteArray retained_preferences("File.Size=20\nFuture_Field=keep\n");
  write_file(preferences, retained_preferences);
  const auto preferences_report = run(options, 0);
  require(preferences_report.contains(QStringLiteral("Settings: ") + preferences) &&
          preferences_report.contains(QStringLiteral("Future_Field")) &&
          preferences_file.open(QIODevice::ReadOnly) && preferences_file.readAll() == retained_preferences,
          QStringLiteral("Native settings path, retained fields or read-only startup were lost"));
  preferences_file.close();
  require(preferences_file.remove(), QStringLiteral("Could not remove native settings fixture"));
  for (const QString& option : {QStringLiteral("--config-dir"),
                                QStringLiteral("--user-data-dir"),
                                QStringLiteral("--wnn-data-dir")}) {
    require(run({option, QString(), QStringLiteral("--resource-report")}, 2)
                .contains(QStringLiteral("Directory must not be empty")),
            QStringLiteral("Empty directory was accepted"));
  }

  write_file(config + QStringLiteral("/wnn.dat"),
             QByteArray::fromHex("a224b0a1b0a20a"));
  require(run(options, 1).contains(QStringLiteral("Could not load WNN conversion resources")),
          QStringLiteral("Partial auto-detected WNN installation was ignored"));
  write_file(config + QStringLiteral("/wnn.dix"),
             QByteArray::fromHex("a280807700000000"));
  const QString loaded = run(options, 0);
  require(loaded.contains(QStringLiteral("WNN conversion: loaded (1 records)")) &&
              loaded.contains(personal + QStringLiteral("/user.sel")) &&
              loaded.contains(personal + QStringLiteral("/user.cnv")) &&
              QDir(personal).exists(),
          QStringLiteral("WNN autoload or personal directory failed: ") + loaded);
  require(run(options + QStringList{QStringLiteral("--wnn-data-dir"),
                                    root + QStringLiteral("/absent")}, 1)
              .contains(QStringLiteral("Could not load WNN conversion resources")),
          QStringLiteral("Invalid explicit WNN override was ignored"));

  const QString external = root + QStringLiteral("/external WNN");
  write_file(external + QStringLiteral("/wnn.dix"),
             QByteArray::fromHex("a280807700000000"));
  write_file(external + QStringLiteral("/wnn.dat"),
             QByteArray::fromHex("a224b0a1b0a20a"));
  write_file(config + QStringLiteral("/wnn.dix"), QByteArray("broken"));
  require(run(options + QStringList{QStringLiteral("--wnn-data-dir"), external}, 0)
              .contains(QStringLiteral("WNN conversion: loaded (1 records)")),
          QStringLiteral("Explicit WNN directory did not override config"));
  const QString blocked = root + QStringLiteral("/not a directory");
  write_file(blocked, QByteArray("preserve"));
  require(run({QStringLiteral("--config-dir"), blocked,
               QStringLiteral("--resource-report")}, 1)
              .contains(QStringLiteral("Could not create the jwpqt configuration directory")),
          QStringLiteral("Invalid config directory was accepted"));
  require(run({QStringLiteral("--config-dir"), config,
               QStringLiteral("--user-data-dir"), blocked,
               QStringLiteral("--wnn-data-dir"), external,
               QStringLiteral("--resource-report")}, 1)
              .contains(QStringLiteral("Could not create the jwpqt user data directory")),
          QStringLiteral("Invalid personal directory was accepted"));
  QFile preserved(desktop_settings);
  require(preserved.open(QIODevice::ReadOnly) && preserved.readAll() == settings,
          QStringLiteral("Desktop settings were modified"));
}

QPalette menu_palette(bool dark) {
  QPalette palette = QApplication::style()->standardPalette();
  const QColor background = dark ? QColor(32, 35, 37) : QColor(239, 239, 239);
  const QColor foreground = dark ? QColor(240, 240, 240) : QColor(24, 24, 24);
  for (const auto role : {QPalette::Window, QPalette::Button,
                          QPalette::Base, QPalette::AlternateBase}) {
    palette.setColor(role, background);
  }
  for (const auto role : {QPalette::WindowText, QPalette::ButtonText,
                          QPalette::Text}) {
    palette.setColor(role, foreground);
    palette.setColor(QPalette::Disabled, role,
                     dark ? QColor(150, 150, 150) : QColor(100, 100, 100));
  }
  return palette;
}

void require_menu_text(QMenuBar* bar, const QColor& foreground,
                      QAction* only = nullptr) {
  const QImage menus = bar->grab().toImage();
  for (QAction* action : bar->actions()) {
    if (only != nullptr && action != only) continue;
    int foreground_pixels = 0;
    const QRect logical = bar->actionGeometry(action);
    const qreal scale = menus.devicePixelRatio();
    const QRect box = QRect(logical.topLeft() * scale, logical.size() * scale)
                          .intersected(menus.rect());
    for (int y = box.top(); y <= box.bottom(); ++y) {
      for (int x = box.left(); x <= box.right(); ++x) {
        const QColor pixel = menus.pixelColor(x, y);
        if (std::abs(pixel.red() - foreground.red()) < 12 &&
            std::abs(pixel.green() - foreground.green()) < 12 &&
            std::abs(pixel.blue() - foreground.blue()) < 12) {
          ++foreground_pixels;
        }
      }
    }
    require(foreground_pixels >= 3,
            QStringLiteral("Menu label has no visible foreground: ") + action->text());
  }
}

void test_visible_menus(bool dark) {
  const QPalette palette = menu_palette(dark);
  QApplication::setPalette(palette);
  jwpqt::qt::MainWindow window;
  window.show();
  QApplication::processEvents();
  require_menu_text(window.menuBar(), palette.color(QPalette::WindowText));
  auto* resources = window.findChild<QToolButton*>(QStringLiteral("resourceStatus"));
  require(resources != nullptr && resources->isVisible() &&
              resources->text() == QStringLiteral("Resources: incomplete"),
          QStringLiteral("Missing resources were not visible in the status bar"));
  bool saw_report = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (dialog != nullptr) {
      saw_report = dialog->textFormat() == Qt::PlainText &&
                   dialog->text().contains(QStringLiteral("WNN conversion: unavailable"));
      dialog->accept();
    }
  });
  resources->click();
  require(saw_report, QStringLiteral("Resource status button did not explain missing data"));
  const QString screenshot = QDir(QCoreApplication::applicationDirPath()).filePath(
      dark ? QStringLiteral("runtime-dark.png") : QStringLiteral("runtime-light.png"));
  require(window.grab().save(screenshot),
          QStringLiteral("Could not capture runtime UI: ") + screenshot);
}

void test_menu_palette_changes() {
  jwpqt::qt::MainWindow window;
  window.show();
  auto* bar = window.menuBar();
  QAction* first = bar->actions().front();
  for (const bool dark : {true, false, true}) {
    QPalette palette = menu_palette(dark);
    const QColor background = palette.color(QPalette::Window);
    palette.setColor(QPalette::WindowText, background);
    palette.setColor(QPalette::ButtonText, background);
    palette.setColor(QPalette::Highlight,
                     dark ? QColor(60, 80, 95) : QColor(190, 215, 230));
    palette.setColor(QPalette::HighlightedText, palette.color(QPalette::Highlight));
    QApplication::setPalette(palette);
    QApplication::processEvents();
    require_menu_text(bar, palette.color(QPalette::Text));
    require(window.palette().color(QPalette::WindowText) == background &&
                window.findChild<QTextEdit*>()->palette().color(QPalette::Text) ==
                    palette.color(QPalette::Text) &&
                first->menu()->styleSheet().isEmpty() &&
                first->menu()->palette().color(QPalette::Text) ==
                    palette.color(QPalette::Text),
            QStringLiteral("Menu contrast repair leaked into other widgets"));
    bar->setActiveAction(first);
    QApplication::processEvents();
    require_menu_text(bar, palette.color(QPalette::Text), first);
    first->menu()->hide();
    bar->setActiveAction(nullptr);
    first->setEnabled(false);
    require_menu_text(bar, palette.color(QPalette::Disabled, QPalette::Text), first);
    first->setEnabled(true);
  }
  require(window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath(
              QStringLiteral("runtime-menu-contrast.png"))),
          QStringLiteral("Could not capture repaired menu contrast"));
  const QPoint point = bar->actionGeometry(first).center();
  for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent mouse(type, point, bar->mapToGlobal(point), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(bar, &mouse);
  }
  require(first->menu()->isVisible(),
          QStringLiteral("Styled menu no longer opens on click"));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(first->menu(), &escape);
  require(!first->menu()->isVisible(),
          QStringLiteral("Styled menu no longer closes with Escape"));
  bar->setActiveAction(nullptr);

  for (const bool dark : {false, true}) {
    QPalette palette = menu_palette(dark);
    for (const auto role : {QPalette::WindowText, QPalette::ButtonText, QPalette::Text}) {
      palette.setColor(role, palette.color(QPalette::Window));
    }
    QApplication::setPalette(palette);
    QApplication::processEvents();
    require_menu_text(bar, dark ? QColor(Qt::white) : QColor(Qt::black));
  }
}

void test_toolbar_icon_theme(const QString& root) {
  const QString old_theme = QIcon::themeName();
  const QString old_fallback = QIcon::fallbackThemeName();
  const QStringList old_paths = QIcon::themeSearchPaths();
  const QStringList old_fallback_paths = QIcon::fallbackSearchPaths();
  const QString theme = root + QStringLiteral("/icons/low-contrast");
  require(QDir().mkpath(theme + QStringLiteral("/16x16/actions")),
          QStringLiteral("Could not create test icon theme"));
  write_file(theme + QStringLiteral("/index.theme"),
             "[Icon Theme]\nName=Low Contrast\nDirectories=16x16/actions\n"
             "[16x16/actions]\nSize=16\nType=Fixed\nContext=Actions\n");
  QImage monochrome(16, 16, QImage::Format_ARGB32);
  monochrome.fill(Qt::transparent);
  QImage colorful = monochrome;
  for (int y = 2; y < 14; ++y) {
    for (int x = 2; x < 14; ++x) {
      monochrome.setPixelColor(x, y, Qt::black);
      colorful.setPixelColor(x, y, x < 8 ? Qt::cyan : Qt::yellow);
    }
  }
  require(monochrome.save(theme + QStringLiteral("/16x16/actions/edit-cut.png")) &&
              colorful.save(theme + QStringLiteral("/16x16/actions/edit-copy.png")),
          QStringLiteral("Could not create test toolbar icons"));
  QIcon::setThemeSearchPaths({root + QStringLiteral("/icons")});
  QIcon::setFallbackSearchPaths({});
  QIcon::setFallbackThemeName({});
  QIcon::setThemeName(QStringLiteral("low-contrast"));
  {
    jwpqt::qt::MainWindow window;
    window.show();
    auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("mainToolBar"));
    auto* cut = window.findChild<QAction*>(QStringLiteral("cutAction"));
    auto* copy = window.findChild<QAction*>(QStringLiteral("copyAction"));
    auto* editor = window.findChild<QTextEdit*>();
    editor->insertPlainText(QStringLiteral("abc"));
    editor->selectAll();
    require(!cut->icon().isNull() && !copy->icon().isNull(),
            QStringLiteral("Test icon theme was not loaded"));
    for (const bool dark : {true, false, true}) {
      QApplication::setPalette(menu_palette(dark));
      QApplication::processEvents();
      for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected}) {
        for (const auto state : {QIcon::Off, QIcon::On}) {
          const QImage icon = cut->icon().pixmap(16, 16, mode, state).toImage();
          const QColor ink = icon.pixelColor(8, 8);
          require(dark ? ink.lightness() > 180 : ink.lightness() < 80,
                  QStringLiteral("Standard toolbar icon is invisible: dark=%1 mode=%2 state=%3 ink=%4")
                      .arg(dark).arg(mode).arg(state).arg(ink.name()));
          require(icon.pixelColor(0, 0).alpha() == 0,
                  QStringLiteral("Toolbar icon adaptation lost transparency"));
        }
      }
      const QImage color = copy->icon().pixmap(16, 16).toImage();
      require(color.pixelColor(4, 8) == QColor(Qt::cyan) &&
                  color.pixelColor(12, 8) == QColor(Qt::yellow),
              QStringLiteral("Healthy multicolor toolbar artwork was recolored"));
      const QColor disabled = cut->icon().pixmap(16, 16, QIcon::Disabled).toImage().pixelColor(8, 8);
      require(std::abs(disabled.lightness() - toolbar->palette().color(QPalette::Window).lightness()) > 60,
              QStringLiteral("Disabled standard toolbar icon has no visible contrast"));
      require(toolbar->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath(
                  dark ? QStringLiteral("toolbar-themed-dark.png")
                       : QStringLiteral("toolbar-themed-light.png"))),
              QStringLiteral("Could not capture themed toolbar"));
    }
  }
  QIcon::setThemeName(old_theme);
  QIcon::setFallbackThemeName(old_fallback);
  QIcon::setThemeSearchPaths(old_paths);
  QIcon::setFallbackSearchPaths(old_fallback_paths);
}

void test_toolbar(const QString& root) {
  test_toolbar_icon_theme(root);
  const QString old_theme = QIcon::themeName();
  const QString old_fallback = QIcon::fallbackThemeName();
  const QStringList old_paths = QIcon::themeSearchPaths();
  const QStringList old_fallback_paths = QIcon::fallbackSearchPaths();
  QIcon::setThemeName(QStringLiteral("jwpqt-no-icon-theme"));
  QIcon::setFallbackThemeName(QString());
  QIcon::setThemeSearchPaths({});
  QIcon::setFallbackSearchPaths({});
  jwpqt::qt::MainWindow window;
  window.setAnimated(false);
  window.resize(1500, 680);
  window.show();
  QApplication::processEvents();
  auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("mainToolBar"));
  auto* editor = window.findChild<QTextEdit*>();
  require(toolbar != nullptr && toolbar->isVisible() && editor != nullptr,
          QStringLiteral("Default native toolbar is missing"));
  const QStringList expected{
      "newDocumentAction", "openDocumentAction", "saveDocumentAction",
      "printAction", "cutAction", "copyAction", "pasteAction", "undoAction",
      "redoAction", "findAction", "replaceAction", "findNextAction",
      "kanaInputAction", "asciiInputAction", "jasciiInputAction",
      "convertSelectionAction", "kanjiInfoAction", "jisTableAction",
      "edictLookupAction", "kanjiCountAction", "radicalLookupAction",
      "bushuLookupAction", "strokeBushuLookupAction", "skipLookupAction",
      "spahnLookupAction", "fourCornerLookupAction", "kanjiReadingLookupAction",
      "indexLookupAction",
      "pageLayoutAction"};
  QStringList actual;
  int separators = 0;
  for (QAction* action : toolbar->actions()) {
    if (action->isSeparator()) {
      ++separators;
      continue;
    }
    actual << action->objectName();
    auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(action));
    bool in_menu = false;
    for (QMenu* menu : window.findChildren<QMenu*>()) {
      in_menu = in_menu || menu->actions().contains(action);
    }
    require(action == window.findChild<QAction*>(action->objectName()) &&
                in_menu && button != nullptr && button->defaultAction() == action &&
                !button->toolTip().isEmpty() &&
                (!button->icon().isNull() || !button->text().isEmpty()),
            QStringLiteral("Toolbar did not reuse a labeled menu action: ") + action->objectName());
  }
  require(actual == expected && separators == 8,
          QStringLiteral("Toolbar order does not match the supported legacy groups"));
  const auto button = [&](const char* name) {
    QAction* action = window.findChild<QAction*>(QString::fromLatin1(name));
    auto* result = qobject_cast<QToolButton*>(toolbar->widgetForAction(action));
    require(result != nullptr, QStringLiteral("Missing toolbar button: ") + name);
    return result;
  };
  require(button("undoAction")->icon().isNull() &&
              button("undoAction")->text() == QStringLiteral("Undo") &&
              !button("undoAction")->isEnabled() &&
              !button("edictLookupAction")->isEnabled() &&
              !button("radicalLookupAction")->isEnabled() &&
              button("kanaInputAction")->isChecked(),
          QStringLiteral("Iconless toolbar fallback or initial action state is wrong"));
  button("asciiInputAction")->click();
  require(button("asciiInputAction")->isChecked() &&
              !button("kanaInputAction")->isChecked() &&
              !button("jasciiInputAction")->isChecked(),
          QStringLiteral("Toolbar input modes are not exclusive"));
  editor->insertPlainText(QStringLiteral("abc"));
  require(button("undoAction")->isEnabled(), QStringLiteral("Toolbar undo did not enable"));
  button("undoAction")->click();
  require(editor->toPlainText().isEmpty() && button("redoAction")->isEnabled(),
          QStringLiteral("Toolbar undo bypassed document history"));
  button("redoAction")->click();
  editor->selectAll();
  require(button("copyAction")->isEnabled() && button("cutAction")->isEnabled(),
          QStringLiteral("Toolbar clipboard actions did not track the selection"));
  button("copyAction")->click();
  require(QApplication::clipboard()->text() == QStringLiteral("abc"),
          QStringLiteral("Toolbar copy did not publish selected text: ") +
              QApplication::clipboard()->text());
  button("cutAction")->click();
  require(editor->toPlainText().isEmpty(), QStringLiteral("Toolbar cut failed"));
  require(QApplication::clipboard()->text() == QStringLiteral("abc") &&
              editor->canPaste() && button("pasteAction")->isEnabled(),
          QStringLiteral("Toolbar cut lost clipboard data: [") +
              QApplication::clipboard()->text() + ']');
  button("pasteAction")->click();
  require(editor->toPlainText() == QStringLiteral("abc"),
          QStringLiteral("Toolbar paste failed: [") + editor->toPlainText() +
              "] " + window.statusBar()->currentMessage());

  const QString path = root + QStringLiteral("/toolbar.jwp");
  require(window.save_path(path), QStringLiteral("Toolbar save fixture failed"));
  editor->insertPlainText(QStringLiteral("x"));
  button("saveDocumentAction")->click();
  jwpqt::qt::MainWindow reopened;
  require(!window.document_modified() &&
              reopened.open_jwp_path(path, jwpqt::core::kDefaultLegacyCodePage,
                                     jwpqt::qt::OpenMode::kNonInteractive) &&
              reopened.findChild<QTextEdit*>()->toPlainText() == QStringLiteral("abcx"),
          QStringLiteral("Toolbar save did not persist the edited document"));
  button("newDocumentAction")->click();
  window.findChild<QAction*>(QStringLiteral("newTextDocumentAction"))->trigger();
  require(!button("kanaInputAction")->isEnabled() &&
              button("jisTableAction")->isEnabled() &&
              !button("pageLayoutAction")->isEnabled(),
          QStringLiteral("Toolbar state diverged in Unicode text mode"));
  button("jisTableAction")->click();
  auto* unicode_table = window.findChild<QDialog*>(QStringLiteral("jisTableDialog"));
  require(unicode_table != nullptr, QStringLiteral("Unicode toolbar did not open the JIS table"));
  unicode_table->close();
  button("newDocumentAction")->click();
  require(button("jisTableAction")->isEnabled() &&
              button("pageLayoutAction")->isEnabled(),
          QStringLiteral("Toolbar state did not return for a new Japanese document"));
  editor = window.active_editor();
  window.findChild<QAction*>(QStringLiteral("jasciiInputAction"))->trigger();
  require(button("jasciiInputAction")->isChecked() &&
              !button("asciiInputAction")->isChecked(),
          QStringLiteral("Menu mode changes did not update toolbar checks"));

  editor->insertPlainText(QStringLiteral("\u3042"));
  editor->selectAll();
  require(!button("convertSelectionAction")->isEnabled(),
          QStringLiteral("Toolbar conversion enabled without WNN"));
  const QString data = root + QStringLiteral("/toolbar-wnn.dat");
  const QString index = root + QStringLiteral("/toolbar-wnn.dix");
  write_file(data, QByteArray::fromHex("a22ab0a12fb0a20a"));
  write_file(index, QByteArray::fromHex("a280807700000000"));
  require(window.load_wnn_resources(index, data, root + QStringLiteral("/toolbar-user.sel"),
                                    jwpqt::qt::OpenMode::kNonInteractive),
          QStringLiteral("Toolbar WNN fixture did not load"));
  require(button("convertSelectionAction")->isEnabled(),
          QStringLiteral("Toolbar did not react to newly loaded conversion resources"));
  button("convertSelectionAction")->click();
  require(window.conversion_active() && !button("undoAction")->isEnabled() &&
              !button("printAction")->isEnabled(),
          QStringLiteral("Toolbar conversion state: active=%1 undo=%2 print=%3: %4")
              .arg(window.conversion_active()).arg(button("undoAction")->isEnabled())
              .arg(button("printAction")->isEnabled()).arg(window.statusBar()->currentMessage()));
  require(window.accept_conversion() && button("undoAction")->isEnabled(),
          QStringLiteral("Toolbar conversion did not restore undo"));
  button("undoAction")->click();
  require(editor->toPlainText() == QStringLiteral("\u3042"),
          QStringLiteral("Toolbar conversion did not undo as one edit"));

  QAction* visibility = window.findChild<QAction*>(QStringLiteral("showToolbarAction"));
  require(visibility == toolbar->toggleViewAction() && visibility->isChecked(),
          QStringLiteral("Toolbar visibility action is not shared"));
  visibility->trigger();
  require(!toolbar->isVisible() && !visibility->isChecked(), QStringLiteral("Toolbar hide failed"));
  visibility->trigger();
  require(toolbar->isVisible() && visibility->isChecked(), QStringLiteral("Toolbar restore failed"));
  for (const bool dark : {false, true}) {
    QApplication::setPalette(menu_palette(dark));
    QApplication::processEvents();
    for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
      const auto group = mode == QIcon::Normal ? QPalette::Active : QPalette::Disabled;
      const QColor ink = toolbar->palette().color(group, QPalette::ButtonText);
      const QImage icon = button("kanaInputAction")->icon().pixmap(16, 16, mode).toImage();
      int visible_ink = 0;
      int transparent = 0;
      for (int y = 0; y < icon.height(); ++y) {
        for (int x = 0; x < icon.width(); ++x) {
          const QColor pixel = icon.pixelColor(x, y);
          if (pixel == ink) ++visible_ink;
          if (pixel.alpha() == 0) ++transparent;
        }
      }
      require(visible_ink > 10 && transparent > 10,
              QStringLiteral("Toolbar artwork lost its theme ink or transparent background"));
    }
    require(window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath(
                dark ? QStringLiteral("toolbar-dark.png") : QStringLiteral("toolbar-light.png"))),
            QStringLiteral("Could not capture toolbar UI"));
  }
  window.resize(360, 680);
  QApplication::processEvents();
  auto* overflow = toolbar->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
  QAction* overflow_mode = window.findChild<QAction*>(QStringLiteral("asciiInputAction"));
  require(overflow != nullptr && overflow->isVisible() && !button("asciiInputAction")->isVisible(),
          QStringLiteral("Narrow toolbar did not expose overflow"));
  bool used_overflow = false;
  QTimer::singleShot(0, [&] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    if (menu != nullptr) {
      used_overflow = menu->actions().contains(overflow_mode);
      if (used_overflow) overflow_mode->trigger();
      menu->close();
    }
  });
  overflow->click();
  QApplication::processEvents();
  if (overflow->menu() == nullptr && button("asciiInputAction")->isVisible()) {
    button("asciiInputAction")->click();
    used_overflow = true;
  }
  require(used_overflow && button("asciiInputAction")->isChecked(),
          QStringLiteral("Overflow failed: used=%1 checked=%2 visible=%3 menu=%4 popup=%5")
              .arg(used_overflow).arg(button("asciiInputAction")->isChecked())
              .arg(button("asciiInputAction")->isVisible()).arg(overflow->menu() != nullptr)
              .arg(QApplication::activePopupWidget() != nullptr));
  QIcon::setThemeName(old_theme);
  QIcon::setFallbackThemeName(old_fallback);
  QIcon::setThemeSearchPaths(old_paths);
  QIcon::setFallbackSearchPaths(old_fallback_paths);
}

void test_real_resources(const QString& root, const QString& source,
                         const QString& bitmap) {
  using jwpqt::qt::OpenMode;
  const QDir data(source);
  QFile original(data.filePath(QStringLiteral("JWPxp.dic")));
  require(original.open(QIODevice::ReadOnly),
          QStringLiteral("Recovered JWPxp.dic is missing"));
  const QByteArray original_bytes = original.readAll();
  auto registry = jwpqt::core::parse_edict_registry(original_bytes.toStdString());
  for (auto& entry : registry.entries) {
    if (entry.special == jwpqt::core::EdictRegistrySpecial::kUser) {
      entry.path = (root + QStringLiteral("/real-data/user.dct")).toStdU16String();
      continue;
    }
    const QString path = QString::fromStdU16String(entry.path);
    entry.path = data.filePath(path == QStringLiteral("enamdict")
                                  ? QStringLiteral("jwpce-1.50/enamdict") : path)
                     .toStdU16String();
  }
  const QString registry_path = root + QStringLiteral("/real-data/dict.cfg");
  write_file(registry_path,
             QByteArray::fromStdString(jwpqt::core::serialize_edict_registry(registry)));
  jwpqt::qt::MainWindow window;
  require(window.load_edict_configuration(registry_path, OpenMode::kNonInteractive) &&
              window.load_kanji_info(data.filePath(QStringLiteral("kanjinfo.dat")),
                                     OpenMode::kNonInteractive) &&
              window.load_kanji_lookup(data.filePath(QStringLiteral("radical.dat")),
                                       data.filePath(QStringLiteral("stroke.dat")),
                                       bitmap, OpenMode::kNonInteractive) &&
              window.load_wnn_resources(data.filePath(QStringLiteral("wnn.dix")),
                                        data.filePath(QStringLiteral("wnn.dat")),
                                        root + QStringLiteral("/real-data/user.sel"),
                                        root + QStringLiteral("/real-data/user.cnv"),
                                        OpenMode::kNonInteractive),
          QStringLiteral("Real runtime data could not be loaded: ") + window.resource_report());
  std::cout << window.resource_report().toStdString() << '\n';
  require(window.edict_resources()->resources.size() == 3 &&
              window.has_kanji_lookup() &&
              window.findChild<QToolButton*>(QStringLiteral("resourceStatus"))->text() ==
                  QStringLiteral("Resources: warnings") &&
              window.resource_report().contains(QStringLiteral("invalid records skipped")),
           QStringLiteral("Recovered dictionaries were only partially loaded"));
  bool dictionary_hit = false;
  const auto query = jwpqt::core::prepare_edict_query(
      jwpqt::core::encode_jwp_text(U"\u65e5\u672c", jwpqt::core::kDefaultLegacyCodePage));
  for (const auto& resource : window.edict_resources()->resources) {
    if (resource.label == QStringLiteral("EDICT") && resource.index.has_value()) {
      dictionary_hit = !jwpqt::core::search_edict_direct(
          resource.dictionary, *resource.index, query).empty();
    }
  }
  require(dictionary_hit, QStringLiteral("Real EDICT lookup did not find Japan"));
  window.show();
  QApplication::processEvents();
  auto* editor = window.findChild<QTextEdit*>();
  require(editor != nullptr, QStringLiteral("Editor is missing"));
  for (const char ch : std::string("nihon")) {
    const QString text(QChar::fromLatin1(ch));
    QKeyEvent key(QEvent::KeyPress, text.at(0).toUpper().unicode(),
                   Qt::NoModifier, text);
    QApplication::sendEvent(editor, &key);
  }
  window.findChild<QAction*>(QStringLiteral("toggleInputModeAction"))->trigger();
  window.findChild<QAction*>(QStringLiteral("toggleInputModeAction"))->trigger();
  require(editor->toPlainText() == QStringLiteral("\u306b\u307b\u3093"),
          QStringLiteral("Real-data fresh-start romanized input failed"));
  editor->selectAll();
  require(window.convert_selection(), QStringLiteral("Real WNN conversion did not start"));
  for (int attempt = 0;
       editor->toPlainText() != QStringLiteral("\u65e5\u672c") && attempt < 100;
       ++attempt) {
    require(window.cycle_conversion(), QStringLiteral("Real WNN candidate cycling failed"));
  }
  require(editor->toPlainText() == QStringLiteral("\u65e5\u672c") &&
              window.accept_conversion(),
          QStringLiteral("Real WNN did not offer Japan"));
  const QString document = root + QStringLiteral("/real-data/japan.jwp");
  require(window.save_path(document) && !window.document_modified(),
          QStringLiteral("Converted native document did not save"));
  jwpqt::qt::MainWindow reopened;
  require(reopened.open_jwp_path(document, jwpqt::core::kDefaultLegacyCodePage,
                                 OpenMode::kNonInteractive) &&
              reopened.findChild<QTextEdit*>()->toPlainText() ==
                  QStringLiteral("\u65e5\u672c"),
          QStringLiteral("Converted native document did not reopen"));
  require(window.grab().save(QDir(QCoreApplication::applicationDirPath()).filePath(
              QStringLiteral("runtime-real-data.png"))),
          QStringLiteral("Could not capture real-data UI"));
  auto* information = window.findChild<QAction*>(QStringLiteral("kanjiInfoAction"));
  require(information && information->isEnabled(), QStringLiteral("Character Information is unavailable"));
  information->trigger();
  auto* character = dynamic_cast<jwpqt::qt::KanjiInfoDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiInfoDialog")));
  require(character != nullptr, QStringLiteral("Character Information did not open"));
  QApplication::clipboard()->setText(QStringLiteral("\u611b"));
  character->findChild<QPushButton*>(QStringLiteral("kanjiInfoClipboard"))->click();
  auto* fields = character->findChild<QTableWidget*>(QStringLiteral("kanjiInfoFields"));
  auto* readings = character->findChild<QTextEdit*>(QStringLiteral("kanjiInfoReadings"));
  const auto field = [fields](const QString& name) {
    for (int i = 0; i < fields->rowCount(); ++i)
      if (fields->item(i, 0)->text() == name) return fields->item(i, 1)->text();
    return QString();
  };
  QStringList details;
  for (int i = 0; i < fields->rowCount(); ++i)
    details << fields->item(i, 0)->text() + QStringLiteral(": ") + fields->item(i, 1)->text();
  require(character->code() == 0x3026 && character->character() == U'\u611b' &&
              field(QStringLiteral("JIS Code")) == QStringLiteral("3026 (B0A6)") &&
              field(QStringLiteral("Shift-JIS")) == QStringLiteral("88A4") &&
              field(QStringLiteral("Unicode")) == QStringLiteral("U+611B") &&
              field(QStringLiteral("Strokes")) == QStringLiteral("13") &&
              field(QStringLiteral("Bushu")).contains(QStringLiteral("87 (61)")) &&
              field(QStringLiteral("Grade")) == QStringLiteral("4") &&
              field(QStringLiteral("Frequency")) == QStringLiteral("640") &&
              field(QStringLiteral("Halpern / SKIP")) == QStringLiteral("2492    2-4-9") &&
              field(QStringLiteral("Spahn")) == QStringLiteral("4i10.1    259") &&
              field(QStringLiteral("Four Corners")) == QStringLiteral("2024.7") &&
              field(QStringLiteral("Morohashi")) == QStringLiteral("10947    4.1123") &&
              field(QStringLiteral("Pinyin")) == QStringLiteral("\u00e0i") &&
              field(QStringLiteral("Korean")) == QStringLiteral("ae") &&
              field(QStringLiteral("Nelson")) == QStringLiteral("2829    1927") &&
              readings->toPlainText().contains(QStringLiteral("love\naffection\nfavourite")) &&
              readings->toPlainText().contains(QStringLiteral("-- on-yomi --\n\u30a2\u30a4")) &&
              readings->toPlainText().contains(QStringLiteral("\u3044\u3068(\u3057\u3044)")),
          QStringLiteral("Recovered Love character differs from the reference dialog:\n") +
              details.join(QLatin1Char('\n')) + QLatin1Char('\n') + readings->toPlainText());
  for (const bool dark : {false, true}) {
    QApplication::setPalette(menu_palette(dark));
    QApplication::processEvents();
    require(character->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath(
                dark ? QStringLiteral("character-information-dark.png")
                     : QStringLiteral("character-information-light.png"))),
            QStringLiteral("Could not capture the real Character Information dialog"));
  }
  QTextCursor cursor(readings->document());
  cursor.setPosition(readings->toPlainText().indexOf(QChar(0x30a2)));
  const QRect start = readings->cursorRect(cursor);
  cursor.movePosition(QTextCursor::NextCharacter);
  const QPoint point((start.left() + readings->cursorRect(cursor).left()) / 2,
                      start.center().y());
  QContextMenuEvent nested(QContextMenuEvent::Mouse, point,
      readings->viewport()->mapToGlobal(point), Qt::ShiftModifier);
  QApplication::sendEvent(readings->viewport(), &nested);
  QApplication::processEvents();
  const auto dialogs = window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog"));
  require(dialogs.size() == 2 && character->code() == 0x3026 && character->isVisible() &&
              !window.document_modified() && editor->toPlainText() == QStringLiteral("\u65e5\u672c"),
          QStringLiteral("Real reading lookup did not preserve its original window and document"));
  auto* kana = dynamic_cast<jwpqt::qt::KanjiInfoDialog*>(
      dialogs.front() == character ? dialogs.back() : dialogs.front());
  require(kana && kana->isVisible() && kana->code() == 0x2522 &&
              kana->findChild<QTextEdit*>(QStringLiteral("kanjiInfoReadings"))->toPlainText() ==
                  QStringLiteral("-- romaji --\na"),
          QStringLiteral("Real on-yomi lookup did not open independent kana information"));
  const auto capture = [](QWidget* widget, const QString& name) {
    for (const bool dark : {false, true}) {
      QApplication::setPalette(menu_palette(dark));
      QApplication::processEvents();
      require(widget->grab().save(QDir(QCoreApplication::applicationDirPath()).filePath(
                  name + (dark ? QStringLiteral("-dark.png") : QStringLiteral("-light.png")))),
              QStringLiteral("Could not capture ") + name);
    }
  };
  const auto open = [&window](const char* name) {
    auto* action = window.findChild<QAction*>(QString::fromLatin1(name));
    require(action && action->isEnabled(), QStringLiteral("Real-data action unavailable: ") + name);
    action->trigger();
    QApplication::processEvents();
  };
  open("edictLookupAction");
  auto* dictionary = dynamic_cast<jwpqt::qt::EdictLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("edictLookupDialog")));
  require(dictionary != nullptr, QStringLiteral("Real dictionary dialog did not open"));
  auto* query_edit = dictionary->findChild<QLineEdit*>(QStringLiteral("edictQuery"));
  query_edit->clear();
  for (const char ch : std::string("ai")) {
    const QString text(QChar::fromLatin1(ch));
    QKeyEvent key(QEvent::KeyPress, text.front().toUpper().unicode(), Qt::NoModifier, text);
    QApplication::sendEvent(query_edit, &key);
  }
  require(query_edit->text() == QStringLiteral("\u3042\u3044") && dictionary->search(),
          QStringLiteral("Real dictionary kana-field search failed"));
  auto* dictionary_results = dictionary->findChild<QTextEdit*>(QStringLiteral("edictResults"));
  const int love = dictionary_results->toPlainText().indexOf(QChar(0x611b));
  auto* aggregate = window.findChild<QWidget*>(QStringLiteral("edictResultsWindow"));
  require(love >= 0 && dictionary->isVisible() && (!aggregate || !aggregate->isVisible()) &&
              window.findChildren<QDialog*>(QStringLiteral("edictLookupDialog")).size() == 1,
          QStringLiteral("Real dictionary search lost Love or opened a second results window"));
  QTextCursor result_cursor(dictionary_results->document());
  result_cursor.setPosition(love);
  dictionary_results->setTextCursor(result_cursor);
  dictionary_results->ensureCursorVisible();
  const QRect love_start = dictionary_results->cursorRect(result_cursor);
  result_cursor.movePosition(QTextCursor::NextCharacter);
  const QPoint love_point((love_start.left() + dictionary_results->cursorRect(result_cursor).left()) / 2,
                          love_start.center().y());
  const auto information_count = window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog")).size();
  QContextMenuEvent dictionary_info(QContextMenuEvent::Mouse, love_point,
      dictionary_results->viewport()->mapToGlobal(love_point), Qt::ShiftModifier);
  QApplication::sendEvent(dictionary_results->viewport(), &dictionary_info);
  require(window.findChildren<QDialog*>(QStringLiteral("kanjiInfoDialog")).size() == information_count + 1,
          QStringLiteral("Real dictionary character navigation did not open independent information"));
  capture(dictionary, QStringLiteral("lookup-dictionary"));

  open("jisTableAction");
  auto* table = dynamic_cast<jwpqt::qt::JisTableDialog*>(
      window.findChild<QDialog*>(QStringLiteral("jisTableDialog")));
  require(table && table->set_jis(0x2421), QStringLiteral("Real Character Table did not select hiragana"));
  capture(table, QStringLiteral("lookup-character-table"));

  open("radicalLookupAction");
  auto* radical = dynamic_cast<jwpqt::qt::KanjiLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiLookupDialog")));
  require(radical != nullptr, QStringLiteral("Real radical dialog did not open"));
  const auto bushu_choices = jwpqt::core::kanji_bushu_choices(0, true);
  const auto claw = std::find_if(bushu_choices.begin(), bushu_choices.end(),
      [](const auto& choice) { return choice.bushu == 87; });
  require(claw != bushu_choices.end(), QStringLiteral("Recovered claw radical is missing"));
  radical->set_selected_radicals({claw->sprite_index});
  radical->set_stroke_range(13, 13);
  require(radical->search(), QStringLiteral("Real radical search failed"));
  const auto radical_results = radical->result_codes();
  require(std::find(radical_results.begin(), radical_results.end(), 0x3026) != radical_results.end(),
          QStringLiteral("Real radical search did not find Love"));
  capture(radical, QStringLiteral("lookup-radical"));

  open("bushuLookupAction");
  auto* codes = dynamic_cast<jwpqt::qt::KanjiCodeLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiCodeLookupDialog")));
  require(codes != nullptr, QStringLiteral("Real code lookup dialog did not open"));
  const auto has_love = [codes] {
    const auto results = codes->results();
    return std::any_of(results.begin(), results.end(), [](const auto& match) { return match.code == 0x3026; });
  };
  codes->set_bushu_query({{87, 87}, {13, 13}, true, true});
  require(codes->search_bushu() && has_love(), QStringLiteral("Real Bushu search did not find Love"));
  capture(codes, QStringLiteral("lookup-bushu"));
  open("strokeBushuLookupAction");
  codes->findChild<QSpinBox*>(QStringLiteral("strokeBushuRadicalStrokes"))->setValue(4);
  auto* stroke_choices = codes->findChild<QListWidget*>(QStringLiteral("strokeBushuRadicals"));
  for (int i = 0; i < stroke_choices->count(); ++i)
    if (stroke_choices->item(i)->data(Qt::UserRole).toInt() == 87) {
      stroke_choices->setCurrentRow(i);
      break;
    }
  require(stroke_choices->currentItem() &&
              stroke_choices->currentItem()->data(Qt::UserRole).toInt() == 87,
          QStringLiteral("Stroke/Bushu claw choice is missing"));
  codes->findChild<QSpinBox*>(QStringLiteral("strokeBushuMinimumStrokes"))->setValue(13);
  codes->findChild<QSpinBox*>(QStringLiteral("strokeBushuMaximumStrokes"))->setValue(13);
  require(codes->search_stroke_bushu() && has_love(), QStringLiteral("Real stroke/Bushu search did not find Love"));
  capture(codes, QStringLiteral("lookup-stroke-bushu"));
  open("skipLookupAction");
  codes->set_skip_query({{2, 2}, {4, 4}, {9, 9}, false});
  require(codes->search_skip() && has_love(), QStringLiteral("Real SKIP search did not find Love"));
  capture(codes, QStringLiteral("lookup-skip"));
  open("spahnLookupAction");
  codes->set_spahn_query({{4, 4}, {8, 8}, {10, 10}, {1, 1}});
  require(codes->search_spahn() && has_love(), QStringLiteral("Real Spahn search did not find Love"));
  capture(codes, QStringLiteral("lookup-spahn"));
  open("fourCornerLookupAction");
  codes->set_four_corner_query({{2, 0, 2, 4, 7}});
  require(codes->search_four_corner() && has_love(), QStringLiteral("Real Four Corner search did not find Love"));
  capture(codes, QStringLiteral("lookup-four-corner"));
  open("indexLookupAction");
  codes->set_index_query({jwpqt::core::KanjiIndexType::kNelson, 2829, 0});
  require(codes->search_index() && has_love(), QStringLiteral("Real Nelson index search did not find Love"));
  capture(codes, QStringLiteral("lookup-index"));
  open("kanjiReadingLookupAction");
  auto* reading = dynamic_cast<jwpqt::qt::KanjiReadingLookupDialog*>(
      window.findChild<QDialog*>(QStringLiteral("kanjiReadingLookupDialog")));
  require(reading != nullptr, QStringLiteral("Real reading dialog did not open"));
  reading->set_query_text(U"\u3042\u3044");
  require(reading->search(), QStringLiteral("Real reading lookup failed"));
  const auto readings_result = reading->results();
  require(std::find(readings_result.begin(), readings_result.end(), 0x3026) != readings_result.end(),
          QStringLiteral("Real reading search did not find Love"));
  capture(reading, QStringLiteral("lookup-reading"));
  require(!window.document_modified() && editor->toPlainText() == QStringLiteral("\u65e5\u672c"),
          QStringLiteral("Browsing lookup references mutated the document"));
  editor->selectAll();
  editor->insertPlainText(QStringLiteral("\u3042\u3044"));
  editor->selectAll();
  auto* convert = window.findChild<QAction*>(QStringLiteral("convertSelectionAction"));
  qobject_cast<QToolButton*>(window.findChild<QToolBar*>(QStringLiteral("mainToolBar"))
                                ->widgetForAction(convert))->click();
  require(window.conversion_active(), QStringLiteral("Real Convert toolbar button did not start conversion"));
  auto* candidate_list = window.findChild<QListWidget*>(QStringLiteral("conversionCandidates"));
  const auto love_candidates = candidate_list->findItems(QStringLiteral("\u611b"), Qt::MatchExactly);
  require(!love_candidates.empty(), QStringLiteral("Real conversion strip did not contain Love"));
  candidate_list->setCurrentItem(love_candidates.front());
  require(window.conversion_active() && editor->toPlainText() == QStringLiteral("\u611b"),
          QStringLiteral("Selecting the Love candidate did not preserve the conversion preview"));
  capture(&window, QStringLiteral("conversion-candidates"));
  require(window.accept_conversion(), QStringLiteral("Real Love conversion could not be accepted"));
  std::cout << "Real-data workflow: romanized input -> WNN Japan -> save/reopen; "
               "EDICT indexed lookup; Love metadata and independent character navigation; "
               "all lookup reference modes; conversion candidate strip.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
  try {
    require(argc == 2 || (argc == 5 && QString::fromLocal8Bit(argv[2]) ==
                                          QStringLiteral("--real-data")),
            QStringLiteral("Usage: test executable [--real-data directory radicals.bmp]"));
    QTemporaryDir directory(QDir(QCoreApplication::applicationDirPath()).filePath(
        QStringLiteral("runtime-environment-XXXXXX")));
    require(directory.isValid(), QStringLiteral("Could not create test directory"));
    test_runtime_paths(QString::fromLocal8Bit(argv[1]), directory.path());
    test_visible_menus(false);
    test_visible_menus(true);
    test_menu_palette_changes();
    test_toolbar(directory.path());
    QApplication::setPalette(menu_palette(true));
    if (argc == 5) {
      test_real_resources(directory.path(), QString::fromLocal8Bit(argv[3]),
                          QString::fromLocal8Bit(argv[4]));
    }
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
