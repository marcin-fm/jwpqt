// SPDX-License-Identifier: GPL-2.0-or-later
#include "jwpqt/core/unicode_search.h"
#include <iostream>
#include <stdexcept>
using namespace jwpqt::core;
int main() {
  try {
    auto check = [](bool value) { if (!value) throw std::runtime_error("Unicode search assertion failed"); };
    check(find_unicode_text(U"a\uff21A", U"a").size() == 3);
    JwpSearchOptions exact; exact.ignore_ascii_case = false; exact.jascii_ascii_equivalence = false;
    check(find_unicode_text(U"a\uff21A", U"a", exact).size() == 1);
    check(find_unicode_text(U"\ufeff\u00a0\U0001f600\U0001f600", U"\U0001f600").front().first == 2);
    check(find_unicode_text(U"aaaa", U"aa").size() == 2);
    check(find_unicode_text(U"aaaa", U"aa", {}, 100, nullptr, true).size() == 3);
    check(find_unicode_text(U"\uff01!", U"!").size() == 1);
    check(find_unicode_text(U"abc", U"abcd").empty());
    auto fails = [&](auto run) { bool failed = false; try { run(); } catch (const JwpSearchError&) { failed = true; } check(failed); };
    fails([] { find_unicode_text(U"x", U""); });
    fails([] { find_unicode_text(U"aaa", U"a", {}, 2); });
    fails([] { std::size_t work = 2; find_unicode_text(U"aaa", U"a", {}, 3, &work); });
    fails([] { find_unicode_text(std::u32string(1, 0xd800), U"a"); });
    fails([] { find_unicode_text(U"a", std::u32string(1, 0x110000)); });
    std::size_t work = 4;
    check(find_unicode_text(U"aaa", U"a", {}, 3, &work).size() == 3 && work == 1);
    fails([&] { find_unicode_text(U"aa", U"a", {}, 3, &work); });
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
