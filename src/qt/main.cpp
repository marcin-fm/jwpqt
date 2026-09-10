// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QAction>
#include <QIcon>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include "jwpqt/core/text_file.h"
#include "main_window.h"

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("jwpqt"));
  QCoreApplication::setApplicationVersion(QStringLiteral(JWPQT_VERSION));
  QCoreApplication::setOrganizationName(QStringLiteral("jwpqt"));
  QApplication::setDesktopFileName(QStringLiteral("jwpqt"));
  application.setWindowIcon(QIcon(QStringLiteral(":/jwpqt/mainicon.ico")));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Native Linux port of JWPxp"));
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
          config.filePath(QStringLiteral("dict.cfg")), interaction_mode)) {
    QTextStream(stderr) << "Could not load the dictionary configuration.\n";
    return 1;
  }
  if (!window.load_kanji_info(
          config.filePath(QStringLiteral("kanjinfo.dat")), interaction_mode)) {
    QTextStream(stderr) << "Could not load the kanji information database.\n";
    return 1;
  }
  if (!window.load_kanji_lookup(
          config.filePath(QStringLiteral("radical.dat")),
          config.filePath(QStringLiteral("stroke.dat")),
          config.filePath(QStringLiteral("radicals.bmp")), interaction_mode)) {
    QTextStream(stderr) << "Could not load the radical lookup data.\n";
    return 1;
  }
  const QDir wnn_directory(parser.isSet(wnn_data_directory_option)
                               ? parser.value(wnn_data_directory_option)
                               : config_directory);
  const QFileInfo wnn_index(wnn_directory.filePath(QStringLiteral("wnn.dix")));
  const QFileInfo wnn_data(wnn_directory.filePath(QStringLiteral("wnn.dat")));
  if (parser.isSet(wnn_data_directory_option) || wnn_index.exists() ||
      wnn_index.isSymLink() || wnn_data.exists() || wnn_data.isSymLink()) {
    if (user_data_directory.isEmpty() ||
        !QDir().mkpath(user_data_directory)) {
      QTextStream(stderr)
          << "Could not create the jwpqt user data directory.\n";
      return 1;
    }
    if (!window.load_wnn_resources(
            wnn_index.absoluteFilePath(), wnn_data.absoluteFilePath(),
            QDir(user_data_directory).filePath(QStringLiteral("user.sel")),
            QDir(user_data_directory).filePath(QStringLiteral("user.cnv")),
            interaction_mode)) {
      QTextStream(stderr) << "Could not load WNN conversion resources.\n";
      return 1;
    }
  }
  window.load_previous_session(config.filePath(QStringLiteral("last-session.jpr")), !parser.isSet(resource_report_option));
  if (!window.session_warning().isEmpty()) QTextStream(stderr) << window.session_warning() << '\n';
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
        QTextStream(stderr) << "Could not open " << path << ": ";
        if (!window.project_warning().isEmpty()) {
          QTextStream(stderr) << window.project_warning();
        } else if (encoding.has_value()) {
          QTextStream(stderr) << "the requested encoding failed";
        } else {
          QTextStream(stderr) << "the file type or encoding could not be "
                                 "determined noninteractively";
        }
        QTextStream(stderr) << '\n';
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
