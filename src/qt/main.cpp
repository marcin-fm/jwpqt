// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QAction>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_WASM
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QStatusBar>
#include <emscripten.h>
#endif
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include "jwpqt/core/text_file.h"
#include "application_theme.h"
#include "main_window.h"
#include "vector_artwork.h"

#ifdef Q_OS_WASM
EM_ASYNC_JS(int, jwpqt_sync_web_storage, (int populate), {
  try {
    const root = '/jwpqt-persist';
    if (!FS.analyzePath(root).exists) FS.mkdir(root);
    if (!Module.jwpqtStorageMounted) {
      FS.mount(IDBFS, {}, root);
      Module.jwpqtStorageMounted = true;
    }
    return await new Promise((resolve) => {
      FS.syncfs(!!populate, (error) => {
        if (!error) {
          FS.mkdirTree(root + '/config');
          FS.mkdirTree(root + '/data');
        }
        resolve(error ? 1 : 0);
      });
    });
  } catch (error) {
    console.error('JWPqt browser storage:', error);
    return 1;
  }
});
#endif

namespace {

bool path_exists_or_is_link(const QString& path) {
  const QFileInfo info(path);
  return info.exists() || info.isSymLink();
}

QString packaged_data_directory() {
  if (qEnvironmentVariableIsSet("JWPQT_DISABLE_PACKAGED_DATA")) return {};
#ifdef Q_OS_WASM
  return QDir(QStringLiteral("/jwpqt-data")).exists()
             ? QStringLiteral("/jwpqt-data")
             : QString{};
#else
  const QDir application_directory(QCoreApplication::applicationDirPath());
  const QStringList candidates{
      application_directory.filePath(QStringLiteral("data")),
      application_directory.filePath(QStringLiteral("../share/jwpqt/data")),
      application_directory.filePath(QStringLiteral("../Resources/data"))};
  for (const auto& candidate : candidates) {
    if (QFileInfo(QDir(candidate).filePath(QStringLiteral("kanjinfo.dat"))).isFile())
      return QDir::cleanPath(candidate);
  }
  return {};
#endif
}

QString configured_or_packaged(const QDir& config, const QString& packaged,
                               const QString& name) {
  const QString configured = config.filePath(name);
  if (path_exists_or_is_link(configured) || packaged.isEmpty()) return configured;
  return QDir(packaged).filePath(name);
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef Q_OS_WASM
  qputenv("XDG_CONFIG_HOME", QByteArrayLiteral("/jwpqt-persist/config"));
  qputenv("XDG_DATA_HOME", QByteArrayLiteral("/jwpqt-persist/data"));
  const bool web_storage_available = jwpqt_sync_web_storage(1) == 0;
#endif
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("jwpqt"));
  QCoreApplication::setApplicationVersion(QStringLiteral(JWPQT_VERSION));
  QCoreApplication::setOrganizationName(QStringLiteral("jwpqt"));
  QApplication::setDesktopFileName(QStringLiteral("jwpqt"));
  jwpqt::qt::initialize_application_theme();
#ifdef Q_OS_WASM
  QFile web_font(QStringLiteral(":/jwpqt/assets/fonts/NotoSansJP-wght.ttf"));
  if (web_font.open(QIODevice::ReadOnly)) {
    const int font_id = QFontDatabase::addApplicationFontFromData(web_font.readAll());
    const QStringList families = QFontDatabase::applicationFontFamilies(font_id);
    if (!families.isEmpty()) {
      application.setFont(QFont(families.constFirst()));
    }
  }
  QTimer web_storage_timer;
#endif
  application.setWindowIcon(
      jwpqt::qt::svg_icon(QStringLiteral(":/jwpqt/assets/icons/jwpqt.svg")));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Native Qt port of JWPxp"));
  parser.addHelpOption();
  parser.addVersionOption();
  const QCommandLineOption smoke_test(
      QStringLiteral("smoke-test"),
      QStringLiteral("Start the application and exit after one event cycle."));
  parser.addOption(smoke_test);
  const QCommandLineOption encoding_option(
      {QStringLiteral("e"), QStringLiteral("encoding")},
      QStringLiteral("Text encoding: utf-8, utf-7, utf-16le, utf-16be, jfc, "
                      "euc-jp, shift-jis, new-jis, old-jis, or nec-jis."),
      QStringLiteral("encoding"));
  parser.addOption(encoding_option);
  const QCommandLineOption project_option(
      QStringLiteral("project"),
      QStringLiteral("Open every positional path as a JWP project; --encoding "
                     "is the fallback for ambiguous legacy references."));
  parser.addOption(project_option);
  const QCommandLineOption wnn_data_directory_option(
      QStringLiteral("wnn-data-dir"),
      QStringLiteral("Directory containing wnn.dix and wnn.dat."),
      QStringLiteral("directory"));
  parser.addOption(wnn_data_directory_option);
  const QCommandLineOption config_directory_option(
      QStringLiteral("config-dir"),
      QStringLiteral("Application settings and dictionary directory; does not "
                     "change desktop theme configuration."),
      QStringLiteral("directory"));
  parser.addOption(config_directory_option);
  const QCommandLineOption user_data_directory_option(
      QStringLiteral("user-data-dir"),
      QStringLiteral("Directory for user.sel and user.cnv."),
      QStringLiteral("directory"));
  parser.addOption(user_data_directory_option);
  const QCommandLineOption resource_report_option(
      QStringLiteral("resource-report"),
      QStringLiteral("Load resources, print their status and exit."));
  parser.addOption(resource_report_option);
  const QCommandLineOption handbook_option(QStringLiteral("handbook"),
      QStringLiteral("Open the bundled offline handbook."));
  parser.addOption(handbook_option);
  parser.addPositionalArgument(
      QStringLiteral("file"),
      QStringLiteral("Documents or JWP projects to open in order."),
      QStringLiteral("[file...]"));
  parser.process(application);

  const QStringList positional_arguments = parser.positionalArguments();
  if (parser.isSet(project_option) && positional_arguments.isEmpty()) {
    parser.showHelp(2);
  }

  std::optional<jwpqt::core::TextEncoding> encoding;
  if (parser.isSet(encoding_option)) {
    encoding = jwpqt::core::parse_text_encoding(
        parser.value(encoding_option).toStdString());
  }
  if (parser.isSet(encoding_option) && !encoding.has_value()) {
    QTextStream(stderr) << "Unsupported text encoding: "
                        << parser.value(encoding_option) << '\n';
    return 2;
  }

  for (const auto& option : {config_directory_option,
                             user_data_directory_option,
                             wnn_data_directory_option}) {
    if (parser.isSet(option) && parser.value(option).isEmpty()) {
      QTextStream(stderr) << "Directory must not be empty: --"
                          << option.names().constFirst() << '\n';
      return 2;
    }
  }

  jwpqt::qt::MainWindow window;
#ifdef Q_OS_WASM
  if (!web_storage_available) {
    window.statusBar()->showMessage(
        QObject::tr("Browser storage is unavailable; settings and documents are temporary"),
        8000);
  }
#endif
  const jwpqt::qt::OpenMode interaction_mode =
      parser.isSet(smoke_test) || parser.isSet(resource_report_option)
          ? jwpqt::qt::OpenMode::kNonInteractive
          : jwpqt::qt::OpenMode::kInteractive;
  const QString config_directory =
      parser.isSet(config_directory_option)
          ? parser.value(config_directory_option)
          : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  const QString user_data_directory =
      parser.isSet(user_data_directory_option)
          ? parser.value(user_data_directory_option)
          : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (config_directory.isEmpty() || !QDir().mkpath(config_directory)) {
    QTextStream(stderr)
        << "Could not create the jwpqt configuration directory.\n";
    return 1;
  }
  const QDir config(config_directory);
  const QString packaged_data = packaged_data_directory();
  if (!window.load_application_settings(config.filePath(QStringLiteral("jwpqt.cfg")))) {
    QTextStream(stderr) << window.application_settings_warning() << '\n';
  }
  window.load_query_history(config.filePath(QStringLiteral("query-history.bin")));
  if (!window.query_history_warning().isEmpty())
    QTextStream(stderr) << window.query_history_warning() << '\n';
  if (!window.load_recent_file_configuration(
          config.filePath(QStringLiteral("recent-files.json")))) {
    QTextStream(stderr) << window.recent_file_warning() << '\n';
  }
  if (!window.load_kanji_color_configuration(
          config.filePath(QStringLiteral("settings.ini")),
          config.filePath(QStringLiteral("colkanji.lst")),
          interaction_mode)) {
    QTextStream(stderr) << "Could not load the kanji color configuration.\n";
    return 1;
  }
  if (!window.load_edict_configuration(
          config.filePath(QStringLiteral("dict.cfg")), interaction_mode,
          packaged_data)) {
    QTextStream(stderr) << "Could not load the dictionary configuration.\n";
    return 1;
  }
  if (!window.load_kanji_info(
          configured_or_packaged(config, packaged_data,
                                 QStringLiteral("kanjinfo.dat")),
          interaction_mode)) {
    QTextStream(stderr) << "Could not load the kanji information database.\n";
    return 1;
  }
  const bool configured_lookup =
      path_exists_or_is_link(config.filePath(QStringLiteral("radical.dat"))) ||
      path_exists_or_is_link(config.filePath(QStringLiteral("stroke.dat"))) ||
      path_exists_or_is_link(config.filePath(QStringLiteral("radicals.bmp")));
  const QString lookup_directory =
      configured_lookup || packaged_data.isEmpty() ? config.absolutePath()
                                                   : packaged_data;
  if (!window.load_kanji_lookup(
          QDir(lookup_directory).filePath(QStringLiteral("radical.dat")),
          QDir(lookup_directory).filePath(QStringLiteral("stroke.dat")),
          QDir(lookup_directory).filePath(QStringLiteral("radicals.bmp")),
          interaction_mode)) {
    QTextStream(stderr) << "Could not load the radical lookup data.\n";
    return 1;
  }
  const QDir wnn_directory(parser.isSet(wnn_data_directory_option)
                                ? parser.value(wnn_data_directory_option)
                                : config_directory);
  const QFileInfo wnn_index(wnn_directory.filePath(QStringLiteral("wnn.dix")));
  const QFileInfo wnn_data(wnn_directory.filePath(QStringLiteral("wnn.dat")));
  const bool use_external_wnn = parser.isSet(wnn_data_directory_option) ||
                                wnn_index.exists() || wnn_index.isSymLink() ||
                                wnn_data.exists() || wnn_data.isSymLink();
  if (user_data_directory.isEmpty() || !QDir().mkpath(user_data_directory)) {
    QTextStream(stderr) << "Could not create the jwpqt user data directory.\n";
    return 1;
  }
  const QString wnn_index_path =
      use_external_wnn
          ? wnn_index.absoluteFilePath()
          : QStringLiteral(":/jwpqt/assets/data/wnn.dix");
  const QString wnn_data_path =
      use_external_wnn ? wnn_data.absoluteFilePath()
                       : QStringLiteral(":/jwpqt/assets/data/wnn.dat");
  if (!window.load_wnn_resources(
          wnn_index_path, wnn_data_path,
          QDir(user_data_directory).filePath(QStringLiteral("user.sel")),
          QDir(user_data_directory).filePath(QStringLiteral("user.cnv")),
          interaction_mode)) {
    QTextStream(stderr) << "Could not load WNN conversion resources.\n";
    return 1;
  }
  window.load_previous_session(config.filePath(QStringLiteral("last-session.jpr")), !parser.isSet(resource_report_option));
  if (!window.session_warning().isEmpty()) QTextStream(stderr) << window.session_warning() << '\n';
#ifdef Q_OS_WASM
  const auto persist_web_state = [&window] {
    if (window.application_settings().reload_previous_files)
      (void)window.save_previous_session();
    (void)jwpqt_sync_web_storage(0);
  };
  QObject::connect(&web_storage_timer, &QTimer::timeout, persist_web_state);
  web_storage_timer.start(5000);
  QObject::connect(&application, &QCoreApplication::aboutToQuit,
                   persist_web_state);
#endif
  bool opened_argument = false;
  for (const QString& path : positional_arguments) {
    jwpqt::qt::ProjectOpenOptions project_options;
    project_options.legacy_encoding = encoding;
    project_options.append =
        opened_argument || window.document_count() > 1 ||
        !window.current_path().isEmpty();
    const bool opened =
        parser.isSet(project_option)
            ? window.open_project_path(path, project_options, interaction_mode)
            : encoding.has_value()
                  ? window.open_path(path, *encoding, interaction_mode,
                                     project_options.append)
                  : window.open_path_detected(path, interaction_mode,
                                              project_options.append);
    if (!opened) {
      if (interaction_mode == jwpqt::qt::OpenMode::kNonInteractive) {
        QTextStream error_stream(stderr);
        const QString detail = window.last_open_error();
        const QString prefix = QStringLiteral("Could not open ") + path;
        if (detail.startsWith(prefix)) {
          error_stream << detail;
        } else if (!detail.isEmpty()) {
          error_stream << prefix << ": " << detail;
        } else if (encoding.has_value()) {
          error_stream << prefix << ": the requested encoding failed";
        } else {
          error_stream << prefix
                       << ": the file type or encoding could not be "
                          "determined noninteractively";
        }
        error_stream << '\n';
      }
      continue;
    }
    opened_argument = true;
  }
  if (!positional_arguments.isEmpty() && !opened_argument &&
      interaction_mode == jwpqt::qt::OpenMode::kNonInteractive) {
    return 1;
  }
  if (parser.isSet(resource_report_option)) {
    QTextStream(stdout)
        << "Configuration directory: " << config.absolutePath() << '\n'
        << "User data directory: " << QDir(user_data_directory).absolutePath()
        << '\n' << "WNN directory: " << wnn_directory.absolutePath() << '\n'
        << window.resource_report() << '\n';
    return 0;
  }
  window.show();
  if (!window.open_startup_dictionary(!positional_arguments.isEmpty()))
    QTextStream(stderr) << "Startup dictionary requested but no searchable dictionary is available.\n";
  if (parser.isSet(handbook_option))
    window.findChild<QAction*>(QStringLiteral("helpContentsAction"))->trigger();

  if (parser.isSet(smoke_test)) {
    QTimer::singleShot(0, &window, [&window, &application] {
      if (window.close_application()) application.quit();
    });
  }
  return application.exec();
}
