// SPDX-License-Identifier: GPL-2.0-or-later

#include "kanji_color_settings.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <QSettings>
#include <QString>
#include <QStringList>

namespace jwpqt::qt {
namespace {

constexpr auto kPolicyKey = "kanjiColor/policy";

std::runtime_error settings_error(const std::string& detail) {
  return std::runtime_error("Could not read kanji color settings: " + detail);
}

core::KanjiListColorMode parse_mode(const QString& value) {
  if (value == QStringLiteral("off")) {
    return core::KanjiListColorMode::kOff;
  }
  if (value == QStringLiteral("members")) {
    return core::KanjiListColorMode::kMatch;
  }
  if (value == QStringLiteral("nonmembers")) {
    return core::KanjiListColorMode::kNoMatch;
  }
  throw settings_error("invalid list mode");
}

QString mode_name(core::KanjiListColorMode mode) {
  switch (mode) {
    case core::KanjiListColorMode::kOff:
      return QStringLiteral("off");
    case core::KanjiListColorMode::kMatch:
      return QStringLiteral("members");
    case core::KanjiListColorMode::kNoMatch:
      return QStringLiteral("nonmembers");
  }
  throw std::invalid_argument("Invalid kanji list color mode");
}

std::uint32_t pack_color(core::RgbColor color) noexcept {
  return (static_cast<std::uint32_t>(color.red) << 16U) |
         (static_cast<std::uint32_t>(color.green) << 8U) |
         static_cast<std::uint32_t>(color.blue);
}

core::RgbColor parse_color(const QString& value, const char* name) {
  const bool canonical =
      value.size() == 6 &&
      std::all_of(value.cbegin(), value.cend(), [](QChar character) {
        return (character >= QLatin1Char('0') &&
                character <= QLatin1Char('9')) ||
               (character >= QLatin1Char('a') &&
                character <= QLatin1Char('f'));
      });
  bool valid = false;
  const unsigned int packed = value.toUInt(&valid, 16);
  if (!canonical || !valid) {
    throw settings_error(std::string("invalid ") + name + " color");
  }
  return {static_cast<std::uint8_t>((packed >> 16U) & 0xffU),
          static_cast<std::uint8_t>((packed >> 8U) & 0xffU),
          static_cast<std::uint8_t>(packed & 0xffU)};
}

QString color_name(core::RgbColor color) {
  return QStringLiteral("%1").arg(pack_color(color), 6, 16, QLatin1Char('0'));
}

}  // namespace

core::KanjiColorPolicy read_kanji_color_policy(QSettings& settings) {
  if (settings.status() != QSettings::NoError) {
    throw settings_error("settings backend is unavailable");
  }

  if (!settings.contains(QString::fromLatin1(kPolicyKey))) {
    if (settings.status() != QSettings::NoError) {
      throw settings_error("settings backend is malformed");
    }
    return {};
  }

  const QStringList fields =
      settings.value(QString::fromLatin1(kPolicyKey))
          .toString()
          .split(QLatin1Char(';'), Qt::KeepEmptyParts);
  if (settings.status() != QSettings::NoError) {
    throw settings_error("settings backend is malformed");
  }
  if (fields.size() != 5 || fields[0] != QStringLiteral("v1")) {
    throw settings_error("invalid policy record");
  }

  core::KanjiColorPolicy policy;
  policy.list_mode = parse_mode(fields[1]);
  policy.list_color = parse_color(fields[2], "list");
  if (fields[3] == QStringLiteral("0")) {
    policy.colorize_uncommon = false;
  } else if (fields[3] == QStringLiteral("1")) {
    policy.colorize_uncommon = true;
  } else {
    throw settings_error("invalid uncommon-color toggle");
  }
  policy.uncommon_color = parse_color(fields[4], "uncommon");
  return policy;
}

void write_kanji_color_policy(QSettings& settings,
                              const core::KanjiColorPolicy& policy) {
  const QString mode = mode_name(policy.list_mode);
  const QString record =
      QStringLiteral("v1;%1;%2;%3;%4")
          .arg(mode, color_name(policy.list_color),
               policy.colorize_uncommon ? QStringLiteral("1")
                                        : QStringLiteral("0"),
               color_name(policy.uncommon_color));

  settings.setValue(QString::fromLatin1(kPolicyKey), record);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    throw std::runtime_error("Could not write kanji color settings");
  }
}

}  // namespace jwpqt::qt
