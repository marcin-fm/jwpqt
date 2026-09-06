// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStyle>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

#include "main_window.h"
#include "edict_resources.h"
#include "jwpqt/core/edict_registry.h"
#include "jwpqt/core/edict_search.h"
#include "jwpqt/core/jwp_text_codec.h"

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

void test_visible_menus(bool dark) {
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
  QApplication::setPalette(palette);
  jwpqt::qt::MainWindow window;
  window.show();
  QApplication::processEvents();
  const QImage menus = window.menuBar()->grab().toImage();
  for (QAction* action : window.menuBar()->actions()) {
    int foreground_pixels = 0;
    const QRect box = window.menuBar()->actionGeometry(action)
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
  std::cout << "Real-data workflow: romanized input -> WNN Japan -> save/reopen; "
               "EDICT indexed lookup; kanji/radical resources loaded.\n";
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
