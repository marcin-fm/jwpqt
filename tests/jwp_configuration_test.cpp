#include "jwpqt/core/jwp_configuration.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace jwpqt::core;

void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

template <typename Function>
void rejects(Function function) {
  try {
    function();
  } catch (const JwpConfigurationError&) {
    return;
  }
  throw std::runtime_error("Invalid configuration was accepted");
}

void test_document() {
  const JwpConfigurationKey font{"File.Size", "file_font.size"};
  const std::string source =
      "# original\r\n\t\r\nFile.Size = 16\r\n"
      "Future = opaque \xff \"unfinished\r\n"
      "file_font.size 20\nFILE_FONT.SIZE = 99\rfile.size = 24";
  const auto entries = parse_jwp_configuration(source);
  require(entries.size() == 5 && entries.front().line == 3 &&
              source.substr(entries.front().begin,
                            entries.front().end - entries.front().begin) ==
                  "File.Size = 16\r\n" &&
              jwp_configuration_value(entries, font) == "24",
          "Configuration parsing lost order, aliases, or physical source ranges");
  require(font.matches("FILE.SIZE") && font.matches("file_font.size") &&
              !font.matches("FILE_FONT.SIZE") &&
              !jwp_configuration_value(entries, {"missing", {}}),
          "Configuration name matching changed the legacy case rules");
  const std::vector<JwpConfigurationUpdate> changes{
      {font, "18"}, {{"Show_Toolbar", "toolbar"}, "false"}};
  const auto updated = rewrite_jwp_configuration(source, changes);
  require(updated == "# original\r\n\t\r\nFuture = opaque \xff \"unfinished\r\n"
                     "FILE_FONT.SIZE = 99\rFile.Size = 18\r\nShow_Toolbar = false\r\n" &&
              rewrite_jwp_configuration(updated, changes) == updated &&
              rewrite_jwp_configuration(source, {}) == source,
          "Configuration rewrite lost unknown bytes or accumulated duplicates");
  require(rewrite_jwp_configuration("# no newline", changes).find(
              "# no newline\r\nFile.Size") == 0,
          "Appended settings merged into an unterminated comment");
  rejects([] { (void)parse_jwp_configuration("Key = \n"); });
  rejects([] { (void)parse_jwp_configuration("Key"); });
  rejects([] { (void)parse_jwp_configuration(std::string("#\0x", 3)); });
  rejects([&] { (void)rewrite_jwp_configuration(source, {{font, "x\ny"}}); });
  rejects([&] { (void)rewrite_jwp_configuration(source, {{font, "x"}, {font, "y"}}); });
  rejects([&] {
    (void)rewrite_jwp_configuration(source,
        {{font, "x"}, {{"file_font.size", "different"}, "y"}});
  });
  rejects([&] {
    (void)rewrite_jwp_configuration(source,
        {{font, "x"}, {{"Other", "file_font.size"}, "y"}});
  });
  rejects([] { (void)rewrite_jwp_configuration({}, {{{"Bad Name", {}}, "1"}}); });
  const JwpConfigurationLimits limits{12, 2, 5};
  require(parse_jwp_configuration("A = 1\nB = 2\n", limits).size() == 2,
          "Exact configuration limits were rejected");
  rejects([&] { (void)parse_jwp_configuration("A = 12", limits); });
  rejects([&] { (void)parse_jwp_configuration("#\n#\n#", limits); });
  rejects([&] { (void)parse_jwp_configuration("#1234\n#1234\n#", limits); });
  rejects([&] {
    (void)rewrite_jwp_configuration("#1234\n", {{{"A", {}}, "12"}}, limits);
  });
  rejects([] { (void)parse_jwp_configuration({}, {0, 1, 1}); });
}

void test_numbers() {
  for (const std::string value : {"true", "TRUE", "y", "Yes", "t", "1", "0x1"}) {
    require(parse_jwp_setting_bool(value), "True setting was misread");
  }
  for (const std::string value : {"false", "FALSE", "n", "No", "f", "0", "-0"}) {
    require(!parse_jwp_setting_bool(value), "False setting was misread");
  }
  for (const std::string value : {"", "truth", "Falsehood", "2", "1junk", "on"}) {
    rejects([&] { (void)parse_jwp_setting_bool(value); });
  }
  require(parse_jwp_setting_integer(" -12 ", -32768, 32767) == -12 &&
              parse_jwp_setting_integer("+20", 0, 20) == 20 &&
              parse_jwp_setting_integer("0xFFFFFFFF", 0, 0xffffffffLL) == 0xffffffffLL &&
              parse_jwp_setting_integer("0X80", 0, 255) == 128 &&
              parse_jwp_setting_integer("-9223372036854775808",
                  std::numeric_limits<std::int64_t>::min(), 0) ==
                  std::numeric_limits<std::int64_t>::min(),
          "Signed/hex configuration integer boundary was misread");
  for (const std::string value : {"12x", "0x", "X80", "--1", "1 2", "256", "-1",
                                 "18446744073709551616"}) {
    rejects([&] { (void)parse_jwp_setting_integer(value, 0, 255); });
  }
  rejects([] { (void)parse_jwp_setting_integer("0", 1, 0); });
}

void test_arrays() {
  require(parse_jwp_setting_bytes("ABC $12, 0x345 0Xf 00", 7) ==
              std::vector<std::uint8_t>{0x0a, 0xbc, 0x12, 0x03, 0x45, 0x0f, 0},
          "Legacy odd-nibble binary syntax was misread");
  require(parse_jwp_setting_bytes("0x 01, 0x0X02", 2) ==
              std::vector<std::uint8_t>{1, 2},
          "Legacy binary prefixes were mistaken for data");
  for (const std::string value : {"0x", "X00", "00GG", "1;2", "123", "", "00 00"}) {
    rejects([&] { (void)parse_jwp_setting_bytes(value, 1); });
  }
  require(parse_jwp_setting_string("\"MS Gothic\"", 32) == U"MS Gothic" &&
              parse_jwp_setting_string("\"C:\\font\"", 32) == U"C:\\font" &&
              parse_jwp_setting_string("\"\"", 1).empty() &&
              parse_jwp_setting_string("\"ASCII\"", 6) == U"ASCII" &&
              parse_jwp_setting_string("4100420000000000", 4) == U"AB" &&
              parse_jwp_setting_string("480045004C004C004F000000", 6) == U"HELLO" &&
              parse_jwp_setting_string("4100000000D84200", 4) == U"A" &&
              parse_jwp_setting_string("FFFE3DD800DE0000", 4) == U"\ufeff\U0001f600",
          "Quoted ASCII or fixed UTF-16 strings were misread");
  for (const std::u32string value : {U"", U"A", U"\u65e5\u672c", U"\ufeff\ufffe",
                                      U"\U0001f600", U"a\"b", U"a\nb"}) {
    require(parse_jwp_setting_string(encode_jwp_setting_string(value, 8), 8) == value,
            "Configuration string did not round-trip");
  }
  require(encode_jwp_setting_string(U"ASCII", 6) == "\"ASCII\"",
          "Printable configuration strings were not quoted");
  for (const std::string value : {"\"unterminated", "\"ok\"garbage", "\"a\tb\"",
                                 "4100420043004400",
                                 "00D8000000000000", "00DC000000000000"}) {
    rejects([&] { (void)parse_jwp_setting_string(value, 4); });
  }
  rejects([] { (void)encode_jwp_setting_string(U"AB", 2); });
  rejects([] { (void)encode_jwp_setting_string(U"\U0001f600", 2); });
  rejects([] { (void)encode_jwp_setting_string(std::u32string(1, 0xd800), 4); });
  rejects([] { (void)encode_jwp_setting_string(std::u32string(1, 0), 4); });
  rejects([] { (void)parse_jwp_setting_string("\"\"", 0); });
}
}  // namespace

int main() {
  try {
    test_document();
    test_numbers();
    test_arrays();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
