// SPDX-License-Identifier: GPL-2.0-or-later

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include "kanji_color_settings.h"

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

bool same_policy(const jwpqt::core::KanjiColorPolicy& left,
                 const jwpqt::core::KanjiColorPolicy& right) {
  return left.list_mode == right.list_mode &&
         left.list_color == right.list_color &&
         left.colorize_uncommon == right.colorize_uncommon &&
         left.uncommon_color == right.uncommon_color;
}

void test_defaults_and_round_trip(const QString& directory) {
  const QString path = directory + QStringLiteral("/settings.ini");
  QSettings settings(path, QSettings::IniFormat);
  require(same_policy(jwpqt::qt::read_kanji_color_policy(settings),
                      jwpqt::core::KanjiColorPolicy{}),
          "Missing kanji color settings did not use defaults");

  const jwpqt::core::KanjiColorPolicy expected{
      jwpqt::core::KanjiListColorMode::kNoMatch, {1, 2, 3}, true,
      {253, 254, 255}};
  jwpqt::qt::write_kanji_color_policy(settings, expected);

  QSettings reopened(path, QSettings::IniFormat);
  require(same_policy(jwpqt::qt::read_kanji_color_policy(reopened), expected),
          "Kanji color settings did not round-trip");
}

void test_invalid_settings(const QString& directory) {
  const QString mode_path = directory + QStringLiteral("/bad-mode.ini");
  QSettings bad_mode(mode_path, QSettings::IniFormat);
  bad_mode.setValue(QStringLiteral("kanjiColor/policy"),
                    QStringLiteral("v1;sometimes;0000ff;0;585858"));
  bad_mode.sync();
  bool mode_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_kanji_color_policy(bad_mode));
  } catch (const std::runtime_error&) {
    mode_rejected = true;
  }
  require(mode_rejected, "Invalid kanji list mode was accepted");

  const QString color_path = directory + QStringLiteral("/bad-color.ini");
  QSettings bad_color(color_path, QSettings::IniFormat);
  bad_color.setValue(QStringLiteral("kanjiColor/policy"),
                     QStringLiteral("v1;off;1000000;0;585858"));
  bad_color.sync();
  bool color_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_kanji_color_policy(bad_color));
  } catch (const std::runtime_error&) {
    color_rejected = true;
  }
  require(color_rejected, "Out-of-range kanji list color was accepted");

  const QString signed_color_path =
      directory + QStringLiteral("/signed-color.ini");
  QSettings signed_color(signed_color_path, QSettings::IniFormat);
  signed_color.setValue(QStringLiteral("kanjiColor/policy"),
                        QStringLiteral("v1;off;+000ff;0;585858"));
  signed_color.sync();
  bool signed_color_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_kanji_color_policy(signed_color));
  } catch (const std::runtime_error&) {
    signed_color_rejected = true;
  }
  require(signed_color_rejected,
          "Non-canonical signed kanji list color was accepted");

  const QString toggle_path = directory + QStringLiteral("/bad-toggle.ini");
  QSettings bad_toggle(toggle_path, QSettings::IniFormat);
  bad_toggle.setValue(QStringLiteral("kanjiColor/policy"),
                      QStringLiteral("v1;off;0000ff;yes;585858"));
  bad_toggle.sync();
  bool toggle_rejected = false;
  try {
    static_cast<void>(jwpqt::qt::read_kanji_color_policy(bad_toggle));
  } catch (const std::runtime_error&) {
    toggle_rejected = true;
  }
  require(toggle_rejected, "Malformed uncommon-color toggle was accepted");

  const QString malformed_path = directory + QStringLiteral("/malformed.ini");
  QFile malformed(malformed_path);
  require(malformed.open(QIODevice::WriteOnly) &&
              malformed.write("[broken\nvalue=1\n") > 0,
          "Could not seed malformed settings file");
  malformed.close();
  QSettings malformed_settings(malformed_path, QSettings::IniFormat);
  bool malformed_rejected = false;
  try {
    static_cast<void>(
        jwpqt::qt::read_kanji_color_policy(malformed_settings));
  } catch (const std::runtime_error&) {
    malformed_rejected = true;
  }
  require(malformed_rejected, "Malformed settings backend was accepted");
}

void test_invalid_policy_is_not_written(const QString& directory) {
  const QString path = directory + QStringLiteral("/invalid-write.ini");
  QSettings settings(path, QSettings::IniFormat);
  auto policy = jwpqt::core::KanjiColorPolicy{};
  policy.list_mode = static_cast<jwpqt::core::KanjiListColorMode>(99);
  bool rejected = false;
  try {
    jwpqt::qt::write_kanji_color_policy(settings, policy);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  settings.sync();
  require(rejected && !settings.contains(QStringLiteral("kanjiColor/policy")),
          "Invalid kanji color policy partially changed settings");
}

void test_write_failure(const QString& directory) {
  const QString path = directory + QStringLiteral("/blocked");
  require(QDir().mkpath(path), "Could not create blocked settings path");
  QSettings settings(path, QSettings::IniFormat);
  bool rejected = false;
  try {
    jwpqt::qt::write_kanji_color_policy(
        settings, jwpqt::core::KanjiColorPolicy{});
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  require(rejected, "Unwritable kanji color settings were accepted");
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  QTemporaryDir directory(QDir::tempPath() + QStringLiteral("/jwpqt-kanji-settings-XXXXXX"));
  if (!directory.isValid()) {
    std::cerr << "Could not create temporary settings directory\n";
    return 1;
  }

  try {
    test_defaults_and_round_trip(directory.path());
    test_invalid_settings(directory.path());
    test_invalid_policy_is_not_written(directory.path());
    test_write_failure(directory.path());
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
