// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTimer>

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
  parser.addPositionalArgument(QStringLiteral("file"),
                               QStringLiteral("UTF-8 text file to open."),
                               QStringLiteral("[file]"));
  parser.process(application);

  const QStringList positional_arguments = parser.positionalArguments();
  if (positional_arguments.size() > 1) {
    parser.showHelp(2);
  }

  jwpqt::qt::MainWindow window;
  if (!positional_arguments.isEmpty() &&
      !window.open_path(positional_arguments.constFirst())) {
    return 1;
  }
  window.show();

  if (parser.isSet(smoke_test)) {
    QTimer::singleShot(0, &application, &QCoreApplication::quit);
  }
  return application.exec();
}
