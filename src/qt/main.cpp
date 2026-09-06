// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
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
  QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QCoreApplication::setOrganizationName(QStringLiteral("jwpqt"));

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
      QStringLiteral("Text encoding: utf-8, utf-7, jfc, euc-jp, shift-jis, "
                      "new-jis, old-jis, or nec-jis."),
      QStringLiteral("encoding"));
  parser.addOption(encoding_option);
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
  parser.addPositionalArgument(QStringLiteral("file"),
                               QStringLiteral("Document to open."),
                               QStringLiteral("[file]"));
  parser.process(application);

  const QStringList positional_arguments = parser.positionalArguments();
  if (positional_arguments.size() > 1) {
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
  if (!positional_arguments.isEmpty()) {
    const bool opened =
        encoding.has_value()
            ? window.open_path(positional_arguments.constFirst(), *encoding,
                               interaction_mode)
            : window.open_path_detected(positional_arguments.constFirst(),
                                        interaction_mode);
    if (!opened) {
      if (interaction_mode == jwpqt::qt::OpenMode::kNonInteractive) {
        if (encoding.has_value()) {
          QTextStream(stderr)
              << "Could not open the file using the requested encoding.\n";
        } else {
          QTextStream(stderr)
              << "Could not determine the file encoding noninteractively; "
                 "specify --encoding.\n";
        }
      }
      return 1;
    }
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

  if (parser.isSet(smoke_test)) {
    QTimer::singleShot(0, &application, &QCoreApplication::quit);
  }
  return application.exec();
}
