// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/print_format.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace jwpqt::core;
void require(bool value) { if (!value) throw std::runtime_error("Print formatting assertion failed"); }
void rejects(const std::function<void()>& operation) {
  try { operation(); } catch (const std::invalid_argument&) { return; }
  throw std::runtime_error("Invalid print formatting accepted");
}
int main() {
  try {
    JwpPrintFormatting format;
    const auto text = [&](bool time, int hour, int year = 2024, int month = 2, int day = 3) {
      const auto value = expand_print_pattern(format, time, year, month, day, hour, 5);
      return std::string(value.begin(), value.end());
    };
    require(text(false, 14) == "24/2/3" && text(true, 14) == "2:05 PM");
    require(text(true, 12) == "12:05 AM" && text(true, 0) == "0:05 AM");
    const auto pattern = [&](std::string value) {
      format.patterns[0].assign(value.begin(), value.end()); format.patterns[0].resize(20);
    };
    pattern("&Y &y &M &D && &z&");
    require(text(false, 0, 2004) == "2004 04 2 3 & &z&");
    pattern("&H &h &n &a"); require(text(false, 23) == "23 11 05 PM");
    pattern("&H\t&N"); require(text(false, 23) == "23\t05");
    pattern("&H &h &n &a");
    format.patterns[3] = {'&','Y',0}; format.patterns[3].resize(10);
    require(text(false, 23) == "23 11 05 &Y"); // Suffixes are not recursively expanded.
    format.patterns[0] = {0x3026,'&','D',0}; format.patterns[0].resize(20);
    require(expand_print_pattern(format, false, 2024, 2, 29, 1, 5) == JwpText({0x3026,'2','9'}));
    format.patterns[0][19] = 0xffff; validate_print_formatting(format); // Retained stale tail.
    rejects([&] { (void)text(false, 24); });
    rejects([&] { (void)text(false, 0, 2023, 2, 29); });
    rejects([&] { (void)text(false, 0, 2024, 13); });
    format.position[2] = 1001; rejects([&] { validate_print_formatting(format); });
    format.position[2] = 100;
    format.patterns[0].assign(20, 'A'); rejects([&] { validate_print_formatting(format); });
    format.patterns[0] = {0xffff,0}; format.patterns[0].resize(20);
    rejects([&] { validate_print_formatting(format); });
    std::cout << "Print formatting tests passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
