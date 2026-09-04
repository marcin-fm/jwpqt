#include "jwpqt/core/byte_io.h"
#include "jwpqt/core/jwp_document.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace {

using jwpqt::core::ByteWriter;
using jwpqt::core::JisCode;
using jwpqt::core::JwpDocument;
using jwpqt::core::JwpFormatError;
using jwpqt::core::JwpParagraph;
using jwpqt::core::JwpVersion;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

template <typename Function>
void expect_error(Function function, std::string_view message) {
  try {
    function();
  } catch (const JwpFormatError&) {
    return;
  }
  std::cerr << "FAIL: expected JwpFormatError for " << message << '\n';
  std::exit(1);
}

void write_header(ByteWriter& writer, std::string_view version,
                  std::int16_t paragraphs, std::uint8_t flags = 0,
                  std::int16_t undo = 0) {
  writer.write_u32_le(0x42022667U);
  std::string version_field(6, '\0');
  version.copy(version_field.data(), version.size());
  writer.write_bytes(version_field);
  writer.write_i16_le(paragraphs);
  writer.write_f32_le(1.0F);
  writer.write_f32_le(2.0F);
  writer.write_f32_le(3.0F);
  writer.write_f32_le(4.0F);
  writer.write_u8(flags);
  writer.write_i16_le(undo);
  writer.write_bytes(std::string(97, '\0'));
}

void write_current_paragraph_header(ByteWriter& writer,
                                    std::int16_t text_size,
                                    std::uint8_t flags = 0) {
  writer.write_i16_le(text_size);
  writer.write_i16_le(100);
  writer.write_u8(0);
  writer.write_u8(0);
  writer.write_u8(0);
  writer.write_u8(0);
  writer.write_u8(flags);
  writer.write_bytes(std::string(7, '\0'));
}

void test_minimal_exact_fixture() {
  ByteWriter expected;
  write_header(expected, "J1.20", 1);
  write_current_paragraph_header(expected, 3);
  expected.write_u8('A');
  expected.write_u8(0xa4);
  expected.write_u8(0xa2);
  expected.write_u8('\n');

  const auto document = jwpqt::core::decode_jwp_document(expected.bytes());
  expect(document.source_version == JwpVersion::kJ120,
         "minimal fixture version");
  expect(document.paragraphs.size() == 1, "minimal paragraph count");
  expect(document.paragraphs[0].text ==
             jwpqt::core::JwpText({static_cast<JisCode>('A'), 0x2422}),
         "minimal paragraph content");
  expect(jwpqt::core::encode_jwp_document(document) == expected.bytes(),
         "minimal fixture byte round trip");
}

void test_metadata_formatting_and_escaped_bytes() {
  JwpDocument input;
  input.margins = {0.5F, 1.25F, 2.5F, 3.75F};
  input.landscape = true;
  input.separate_left_right_headers = true;
  input.suppress_first_page_headers = true;
  input.vertical = true;
  input.summary[0] = {0x467c, 0x4b5c};
  input.summary[4] = {static_cast<JisCode>('!')};
  input.headers[3][2] = {0x386c};

  JwpParagraph first;
  first.text = {0x80, 0xff, static_cast<JisCode>('A'), 0x2422, 0x467c};
  first.line_spacing = 125;
  first.first_indent = -7;
  first.left_indent = 2;
  first.right_indent = 3;
  first.page_break = true;
  input.paragraphs.push_back(first);
  input.paragraphs.push_back(JwpParagraph{});

  const std::string bytes = jwpqt::core::encode_jwp_document(input);
  const auto output = jwpqt::core::decode_jwp_document(bytes);

  expect(output.source_version == JwpVersion::kJ120,
         "writer emits current version");
  expect(output.margins == input.margins, "margins round trip");
  expect(output.landscape && output.separate_left_right_headers &&
             output.suppress_first_page_headers && output.vertical,
         "document flags round trip");
  expect(output.summary == input.summary, "summary round trip");
  expect(output.headers == input.headers, "headers round trip");
  expect(output.paragraphs.size() == 2, "formatted paragraph count");
  const auto& result = output.paragraphs.front();
  expect(result.text == first.text, "escaped paragraph data round trip");
  expect(result.line_spacing == 125 && result.first_indent == -7 &&
             result.left_indent == 2 && result.right_indent == 3 &&
             result.page_break,
         "paragraph formatting round trip");
}

void test_old_versions() {
  ByteWriter b1;
  write_header(b1, "B1", 1);
  b1.write_i16_le(2);
  b1.write_i16_le(-2);
  b1.write_i16_le(258);
  b1.write_i16_le(259);
  b1.write_u8('X');
  b1.write_u8('\n');
  const auto old = jwpqt::core::decode_jwp_document(b1.bytes());
  expect(old.source_version == JwpVersion::kB1, "B1 source version");
  expect(old.paragraphs[0].first_indent == -2 &&
             old.paragraphs[0].left_indent == 2 &&
             old.paragraphs[0].right_indent == 3 &&
             old.paragraphs[0].line_spacing == 100,
         "B1 paragraph conversion semantics");

  ByteWriter b2;
  write_header(b2, "B2", 1);
  write_current_paragraph_header(b2, 1, 1);
  b2.write_u8('\n');
  const auto middle = jwpqt::core::decode_jwp_document(b2.bytes());
  expect(middle.source_version == JwpVersion::kB2, "B2 source version");
  expect(middle.paragraphs[0].page_break, "B2 paragraph flags");

  const std::string canonical = jwpqt::core::encode_jwp_document(old);
  expect(canonical.substr(4, 6) == std::string("J1.20\0", 6),
         "old versions save as current format");
}

void test_reserved_bytes_are_canonicalized() {
  ByteWriter canonical_writer;
  write_header(canonical_writer, "J1.20", 1);
  write_current_paragraph_header(canonical_writer, 1, 1);
  canonical_writer.write_u8('\n');
  const std::string canonical = canonical_writer.take_bytes();

  std::string legacy = canonical;
  legacy[28] = static_cast<char>(0xc0U);
  legacy[31] = static_cast<char>(0x5aU);
  legacy[127] = static_cast<char>(0x6bU);
  legacy[135] = static_cast<char>(0x44U);
  legacy[136] = static_cast<char>(0xffU);
  legacy[143] = static_cast<char>(0x99U);

  const auto document = jwpqt::core::decode_jwp_document(legacy);
  expect(document.paragraphs[0].page_break,
         "semantic page-break flag survives reserved bits");
  expect(jwpqt::core::encode_jwp_document(document) == canonical,
         "reserved bytes and flags canonicalize to zero");
}

std::string make_budget_fixture(std::size_t final_paragraph_size) {
  constexpr std::size_t kFullParagraphs = 512;
  constexpr std::size_t kFullParagraphSize = 32766;

  ByteWriter writer;
  write_header(writer, "J1.20",
               static_cast<std::int16_t>(kFullParagraphs + 1U), 0x02U);
  writer.write_i16_le(1);
  writer.write_u16_le(static_cast<std::uint16_t>('m'));
  for (int index = 1; index < 5; ++index) {
    writer.write_i16_le(0);
  }

  const std::string full_text(kFullParagraphSize, 'x');
  for (std::size_t index = 0; index < kFullParagraphs; ++index) {
    write_current_paragraph_header(
        writer, static_cast<std::int16_t>(kFullParagraphSize + 1U));
    writer.write_bytes(full_text);
    writer.write_u8('\n');
  }
  write_current_paragraph_header(
      writer, static_cast<std::int16_t>(final_paragraph_size + 1U));
  writer.write_bytes(std::string(final_paragraph_size, 'y'));
  writer.write_u8('\n');
  return writer.take_bytes();
}

void test_decoded_text_budget() {
  constexpr std::size_t kAtLimitFinalParagraph = 1023;
  {
    const std::string at_limit =
        make_budget_fixture(kAtLimitFinalParagraph);
    const auto document = jwpqt::core::decode_jwp_document(at_limit);
    expect(document.paragraphs.size() == 513,
           "aggregate text at safety limit is accepted");
  }

  const std::string over_limit =
      make_budget_fixture(kAtLimitFinalParagraph + 1U);
  expect_error(
      [&] { jwpqt::core::decode_jwp_document(over_limit); },
      "aggregate metadata and paragraph text above safety limit");
}

void test_invalid_documents() {
  expect_error([] { jwpqt::core::decode_jwp_document(""); },
               "truncated header");

  ByteWriter bad_magic;
  write_header(bad_magic, "J1.20", 0);
  std::string bad_magic_bytes = bad_magic.bytes();
  bad_magic_bytes[0] = 'x';
  expect_error([&] { jwpqt::core::decode_jwp_document(bad_magic_bytes); },
               "bad magic");

  ByteWriter bad_version;
  write_header(bad_version, "NOPE", 0);
  expect_error(
      [&] { jwpqt::core::decode_jwp_document(bad_version.bytes()); },
      "unknown version");

  ByteWriter negative_count;
  write_header(negative_count, "J1.20", -1);
  expect_error(
      [&] { jwpqt::core::decode_jwp_document(negative_count.bytes()); },
      "negative paragraph count");

  ByteWriter undo;
  write_header(undo, "J1.20", 0, 0, 1);
  expect_error([&] { jwpqt::core::decode_jwp_document(undo.bytes()); },
               "native undo data");

  ByteWriter negative_metadata;
  write_header(negative_metadata, "J1.20", 0, 0x02U);
  negative_metadata.write_i16_le(-1);
  expect_error(
      [&] { jwpqt::core::decode_jwp_document(negative_metadata.bytes()); },
      "negative metadata length");

  ByteWriter no_terminator;
  write_header(no_terminator, "J1.20", 1);
  write_current_paragraph_header(no_terminator, 0);
  expect_error(
      [&] { jwpqt::core::decode_jwp_document(no_terminator.bytes()); },
      "zero paragraph length");

  ByteWriter bad_newline;
  write_header(bad_newline, "J1.20", 1);
  write_current_paragraph_header(bad_newline, 1);
  bad_newline.write_u8('X');
  expect_error(
      [&] { jwpqt::core::decode_jwp_document(bad_newline.bytes()); },
      "missing paragraph newline");

  ByteWriter bad_pair;
  write_header(bad_pair, "J1.20", 1);
  write_current_paragraph_header(bad_pair, 2);
  bad_pair.write_u8(0xa1U);
  bad_pair.write_u8(0x20U);
  bad_pair.write_u8('\n');
  expect_error([&] { jwpqt::core::decode_jwp_document(bad_pair.bytes()); },
               "invalid high-bit pair");

  ByteWriter trailing;
  write_header(trailing, "J1.20", 0);
  trailing.write_u8(0);
  expect_error([&] { jwpqt::core::decode_jwp_document(trailing.bytes()); },
               "trailing bytes");
}

void test_invalid_models() {
  JwpDocument bad_character;
  JwpParagraph paragraph;
  paragraph.text = {0x0100};
  bad_character.paragraphs.push_back(paragraph);
  expect_error([&] { jwpqt::core::encode_jwp_document(bad_character); },
               "invalid internal character");

  JwpDocument nul_character;
  paragraph.text = {0};
  nul_character.paragraphs.push_back(paragraph);
  expect_error([&] { jwpqt::core::encode_jwp_document(nul_character); },
               "lossy NUL character");

  JwpDocument long_paragraph;
  paragraph.text.assign(
      static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()),
      static_cast<JisCode>('x'));
  long_paragraph.paragraphs.push_back(std::move(paragraph));
  expect_error([&] { jwpqt::core::encode_jwp_document(long_paragraph); },
               "paragraph size overflow");

  JwpDocument long_metadata;
  long_metadata.summary[0].assign(
      static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()) +
          1U,
      static_cast<JisCode>('x'));
  expect_error([&] { jwpqt::core::encode_jwp_document(long_metadata); },
               "metadata size overflow");

  JwpDocument nul_metadata;
  nul_metadata.summary[0] = {static_cast<JisCode>('A'), 0,
                             static_cast<JisCode>('B')};
  expect_error([&] { jwpqt::core::encode_jwp_document(nul_metadata); },
               "embedded metadata NUL");
}

}  // namespace

int main() {
  test_minimal_exact_fixture();
  test_metadata_formatting_and_escaped_bytes();
  test_old_versions();
  test_reserved_bytes_are_canonicalized();
  test_decoded_text_budget();
  test_invalid_documents();
  test_invalid_models();
  return 0;
}
