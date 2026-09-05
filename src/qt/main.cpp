// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
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
      QStringLiteral("Text encoding: utf-8, euc-jp, shift-jis, "
                      "new-jis, old-jis, or nec-jis."),
      QStringLiteral("encoding"));
  parser.addOption(encoding_option);
  const QCommandLineOption wnn_data_directory_option(
      QStringLiteral("wnn-data-dir"),
      QStringLiteral("Directory containing wnn.dix and wnn.dat."),
      QStringLiteral("directory"));
  parser.addOption(wnn_data_directory_option);
  parser.addPositionalArgument(QStringLiteral("file"),
                               QStringLiteral("Text file to open."),
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

  jwpqt::qt::MainWindow window;
  const jwpqt::qt::OpenMode interaction_mode =
      parser.isSet(smoke_test) ? jwpqt::qt::OpenMode::kNonInteractive
                               : jwpqt::qt::OpenMode::kInteractive;
  const QString config_directory =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
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
  if (parser.isSet(wnn_data_directory_option)) {
    const QDir data_directory(parser.value(wnn_data_directory_option));
    const QString user_data =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (user_data.isEmpty() || !QDir().mkpath(user_data)) {
      QTextStream(stderr)
          << "Could not create the jwpqt user data directory.\n";
      return 1;
    }
    if (!window.load_wnn_resources(
            data_directory.filePath(QStringLiteral("wnn.dix")),
            data_directory.filePath(QStringLiteral("wnn.dat")),
            QDir(user_data).filePath(QStringLiteral("user.sel")),
            QDir(user_data).filePath(QStringLiteral("user.cnv")),
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
  window.show();

  if (parser.isSet(smoke_test)) {
    QTimer::singleShot(0, &application, &QCoreApplication::quit);
  }
  return application.exec();
}
