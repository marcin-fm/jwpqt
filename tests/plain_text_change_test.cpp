#include "jwpqt/core/plain_text_change.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <typename Callback>
void require_change_error(Callback callback, const std::string& message) {
  try {
    callback();
  } catch (const jwpqt::core::PlainTextChangeError&) {
    return;
  }
  throw std::runtime_error(message);
}

void test_replacement() {
  require(jwpqt::core::replace_plain_text_snapshot(U"ABC", U"AXC", 1, 1,
                                                   U"X") == U"AXC",
          "Single-code-point replacement failed");
  require(jwpqt::core::replace_plain_text_snapshot(U"AC", U"ABC", 1, 0,
                                                   U"B") == U"ABC",
          "Insertion failed");
  require(jwpqt::core::replace_plain_text_snapshot(U"ABC", U"AC", 1, 1,
                                                   U"") == U"AC",
          "Deletion failed");
}

void test_non_bmp_change() {
  const std::u32string emoji(1, static_cast<char32_t>(0x1f600));
  std::u32string current = U"AB";
  current.insert(1, emoji);
  require(jwpqt::core::replace_plain_text_snapshot(U"AB", current, 1, 0,
                                                   emoji) == current,
          "Non-BMP insertion was not handled as one Unicode code point");
}

void test_rejects_mismatched_change() {
  require_change_error(
      [] {
        static_cast<void>(jwpqt::core::replace_plain_text_snapshot(
            U"ABC", U"AXC", 1, 1, U"Y"));
      },
      "Mismatched snapshot change was accepted");
  require_change_error(
      [] {
        static_cast<void>(jwpqt::core::replace_plain_text_snapshot(
            U"ABC", U"ABC", 4, 0, U""));
      },
      "Out-of-range snapshot change was accepted");
}

}  // namespace

int main() {
  try {
    test_replacement();
    test_non_bmp_change();
    test_rejects_mismatched_change();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
