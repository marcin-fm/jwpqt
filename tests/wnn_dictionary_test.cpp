#include "jwpqt/core/wnn_dictionary.h"

#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

#include "jwpqt/core/byte_io.h"

namespace {

using jwpqt::core::ByteWriter;
using jwpqt::core::WnnDictionary;
using jwpqt::core::WnnDictionaryError;

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void expect_error(const std::function<void()>& operation,
                  std::string_view context) {
  try {
    operation();
  } catch (const WnnDictionaryError&) {
    return;
  }
  throw std::runtime_error("Expected WNN error for " + std::string(context));
}

std::string bytes(std::initializer_list<std::uint8_t> values) {
  std::string result;
  for (const std::uint8_t value : values) {
    result.push_back(static_cast<char>(value));
  }
  return result;
}

void append_index(ByteWriter& writer,
                  std::initializer_list<std::uint8_t> key,
                  std::uint32_t offset) {
  std::size_t count = 0;
  for (const std::uint8_t value : key) {
    writer.write_u8(value);
    ++count;
  }
  while (count < jwpqt::core::kWnnIndexKeySize) {
    writer.write_u8(0x80);
    ++count;
  }
  writer.write_u8(0x77);
  writer.write_u32_le(offset);
}

struct Fixture {
  std::string index;
  std::string data;
};

Fixture fixture() {
  Fixture result;
  const std::string first = bytes({0xa2, '*', 0xb0, 0xa1, '/', 0xb0, 0xa4,
                                   '\n'});
  const std::string second =
      bytes({0xa2, 0xa4, '1', 0xc1, 0xea, '\n'});
  const std::string third =
      bytes({0xa2, 0xa4, 0xa4, 0xec, 'i', '/', 0xc0, 0xc2, 0xfc, 0x89,
             '\n'});
  result.data = first + second + third;

  ByteWriter index;
  append_index(index, {0xa2}, 0);
  append_index(index, {0xa2, 0xa4},
               static_cast<std::uint32_t>(first.size()));
  append_index(index, {0xa2, 0xa4, 0xa4},
               static_cast<std::uint32_t>(first.size() + second.size()));
  result.index = index.take_bytes();
  return result;
}

void test_parse() {
  const Fixture input = fixture();
  const WnnDictionary dictionary =
      WnnDictionary::parse(input.index, input.data);
  require(dictionary.index().size() == 3, "Wrong WNN index size");
  require(dictionary.records().size() == 3, "Wrong WNN record count");

  const auto& first = dictionary.records()[0];
  require(first.key == std::vector<std::uint8_t>{0xa2},
          "Wrong first WNN key");
  require(first.ending == '*', "Wrong first WNN ending");
  require(first.candidates.size() == 2, "Wrong candidate count");
  require(first.candidates[0] == jwpqt::core::JwpText{0x3021},
          "Wrong first candidate");
  require(first.candidates[1] == jwpqt::core::JwpText{0x3024},
          "Wrong second candidate");

  const auto& third = dictionary.records()[2];
  require(third.key ==
              std::vector<std::uint8_t>({0xa2, 0xa4, 0xa4, 0xec}),
          "Wrong long WNN key");
  require(third.candidates.size() == 2 && third.candidates[0].empty(),
          "Leading empty WNN candidate was not preserved");
  require(third.candidates[1] ==
              jwpqt::core::JwpText({0x4042, 0x7c09}),
          "Wrong candidate after an empty entry");
  require(dictionary.index()[2].record_index == 2,
          "Index did not resolve its record");
}

void test_bad_data() {
  const Fixture valid = fixture();

  std::string missing_newline = valid.data;
  missing_newline.pop_back();
  expect_error(
      [&] { WnnDictionary::parse(valid.index, missing_newline); },
      "unterminated record");

  std::string bad_key = valid.data;
  bad_key[0] = static_cast<char>(0x80);
  expect_error([&] { WnnDictionary::parse(valid.index, bad_key); },
               "invalid data key");

  std::string bad_ending = valid.data;
  bad_ending[1] = '\t';
  expect_error([&] { WnnDictionary::parse(valid.index, bad_ending); },
               "invalid ending");

  std::string partial_pair = valid.data;
  partial_pair.erase(partial_pair.begin() + 3);
  expect_error([&] { WnnDictionary::parse(valid.index, partial_pair); },
               "partial candidate pair");

  std::string ascii_candidate = valid.data;
  ascii_candidate[2] = 'A';
  expect_error([&] { WnnDictionary::parse(valid.index, ascii_candidate); },
               "ASCII candidate byte");

  const std::string unsorted =
      bytes({0xa3, '*', 0xb0, 0xa1, '\n', 0xa2, '*', 0xb0, 0xa1, '\n'});
  expect_error([&] { WnnDictionary::parse(valid.index, unsorted); },
               "unsorted data");
}

void test_legacy_byte_domain_and_empty_candidates() {
  ByteWriter index;
  append_index(index, {0x81}, 0);
  const std::string extended = bytes({0x81, '*', 0x80, 0xff, '\n'});
  const WnnDictionary extended_dictionary =
      WnnDictionary::parse(index.bytes(), extended);
  require(extended_dictionary.records()[0].key ==
              std::vector<std::uint8_t>{0x81},
          "Non-JIS high-bit key was not preserved");
  require(extended_dictionary.records()[0].candidates[0] ==
              jwpqt::core::JwpText{0x007f},
          "Non-JIS high-bit candidate pair was not masked like legacy JWP");

  ByteWriter empty_index;
  append_index(empty_index, {0xa2}, 0);
  const WnnDictionary no_candidates =
      WnnDictionary::parse(empty_index.bytes(), bytes({0xa2, '*', '\n'}));
  require(no_candidates.records()[0].candidates.empty(),
          "Empty candidate list was not preserved");

  const WnnDictionary slash_ending =
      WnnDictionary::parse(empty_index.bytes(), bytes({0xa2, '/', '\n'}));
  require(slash_ending.records()[0].ending == '/' &&
              slash_ending.records()[0].candidates.empty(),
          "Printable slash ending was not preserved");

  const WnnDictionary trailing_empty = WnnDictionary::parse(
      empty_index.bytes(), bytes({0xa2, '*', 0xb0, 0xa1, '/', '\n'}));
  require(trailing_empty.records()[0].candidates.size() == 2 &&
              trailing_empty.records()[0].candidates[0] ==
                  jwpqt::core::JwpText{0x3021} &&
              trailing_empty.records()[0].candidates[1].empty(),
          "Trailing empty WNN candidate was not preserved");
}

void test_resource_limits() {
  ByteWriter index;
  append_index(index, {0xa2}, 0);

  std::string too_many_records;
  const std::string record = bytes({0xa2, '*', 0xb0, 0xa1, '\n'});
  for (std::size_t count = 0;
       count <= jwpqt::core::kWnnMaximumRecordCount; ++count) {
    too_many_records += record;
  }
  expect_error(
      [&] { WnnDictionary::parse(index.bytes(), too_many_records); },
      "record limit");

  std::string too_many_candidates = bytes({0xa2, '*'});
  too_many_candidates.append(jwpqt::core::kWnnMaximumCandidateCount + 1,
                             '/');
  too_many_candidates.push_back('\n');
  expect_error(
      [&] { WnnDictionary::parse(index.bytes(), too_many_candidates); },
      "candidate limit");

  std::string too_many_cells = bytes({0xa2, '*'});
  too_many_cells.reserve(2 *
                             (jwpqt::core::kWnnMaximumCandidateCells + 1) +
                         3);
  for (std::size_t count = 0;
       count <= jwpqt::core::kWnnMaximumCandidateCells; ++count) {
    too_many_cells.push_back(static_cast<char>(0x80));
    too_many_cells.push_back(static_cast<char>(0x80));
  }
  too_many_cells.push_back('\n');
  expect_error([&] { WnnDictionary::parse(index.bytes(), too_many_cells); },
               "candidate-cell limit");
}

void test_bad_index() {
  const Fixture valid = fixture();

  std::string truncated = valid.index;
  truncated.pop_back();
  expect_error([&] { WnnDictionary::parse(truncated, valid.data); },
               "truncated index");

  std::string bad_marker = valid.index;
  bad_marker[3] = 0;
  expect_error([&] { WnnDictionary::parse(bad_marker, valid.data); },
               "index marker");

  std::string bad_padding = valid.index;
  bad_padding[1] = static_cast<char>(0xa2);
  expect_error([&] { WnnDictionary::parse(bad_padding, valid.data); },
               "index padding");

  std::string bad_offset = valid.index;
  bad_offset[4] = 1;
  expect_error([&] { WnnDictionary::parse(bad_offset, valid.data); },
               "non-boundary index offset");

  std::string bad_prefix = valid.index;
  bad_prefix[8] = static_cast<char>(0xa3);
  expect_error([&] { WnnDictionary::parse(bad_prefix, valid.data); },
               "index/data prefix mismatch");

  ByteWriter reordered;
  append_index(reordered, {0xa2}, 0);
  append_index(reordered, {0xa2, 0xa4, 0xa4}, 8);
  append_index(reordered, {0xa2, 0xa4}, 14);
  expect_error(
      [&] { WnnDictionary::parse(reordered.bytes(), valid.data); },
      "unordered index");
}

std::string read_file(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error(std::string("Could not open ") + path);
  }
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

}  // namespace

int main(int argc, char** argv) {
  try {
    test_parse();
    test_bad_data();
    test_legacy_byte_domain_and_empty_candidates();
    test_resource_limits();
    test_bad_index();
    if (argc == 3) {
      const WnnDictionary dictionary =
          WnnDictionary::parse(read_file(argv[1]), read_file(argv[2]));
      require(dictionary.index().size() == 14'537,
              "Unexpected recovered WNN index size");
      require(dictionary.records().size() == 25'496,
              "Unexpected recovered WNN data record count");
    }
  } catch (const std::exception& error) {
    std::cerr << "wnn_dictionary_test: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
