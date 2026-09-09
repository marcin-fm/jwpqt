// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jwp_clipboard.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace jwpqt::core {
namespace {

constexpr std::array<char, 4> kMagic{'J', 'Q', 'C', 'F'};
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kHeaderSize = 12;

bool supported_code_page(std::uint16_t value) noexcept {
  return value >= 1250 && value <= 1258;
}

void write_u16(std::string& output, std::uint16_t value) {
  output.push_back(static_cast<char>(value & 0xffU));
  output.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void write_u32(std::string& output, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    output.push_back(static_cast<char>((value >> shift) & 0xffU));
}

std::uint16_t read_u16(std::string_view bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(
      static_cast<std::uint8_t>(bytes[offset]) |
      (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[offset + 1])) << 8U));
}

std::uint32_t read_u32(std::string_view bytes, std::size_t offset) {
  std::uint32_t value = 0;
  for (unsigned index = 0; index < 4; ++index)
    value |= static_cast<std::uint32_t>(
                 static_cast<std::uint8_t>(bytes[offset + index]))
             << (index * 8U);
  return value;
}

void require_range(const JwpDocumentModel& model, JwpRange range) {
  if (!model.valid_position(range.begin) || !model.valid_position(range.end))
    throw JwpClipboardError("Clipboard selection is outside the document");
  if (range.end < range.begin)
    throw JwpClipboardError("Clipboard selection is reversed");
}

void validate_fragment(const JwpDocument& fragment) {
  if (fragment.paragraphs.empty())
    throw JwpClipboardError("Clipboard fragment has no paragraphs");
  try {
    (void)encode_jwp_document(fragment);
  } catch (const JwpFormatError& error) {
    throw JwpClipboardError(error.what());
  }
}

}  // namespace

JwpDocument copy_jwp_fragment(const JwpDocumentModel& source, JwpRange range) {
  require_range(source, range);
  JwpDocument fragment;
  fragment.margins.fill(1.0F);
  fragment.paragraphs.reserve(range.end.paragraph - range.begin.paragraph + 1);
  for (std::size_t index = range.begin.paragraph; index <= range.end.paragraph;
       ++index) {
    JwpParagraph paragraph = source.paragraph(index);
    const std::size_t begin = index == range.begin.paragraph ? range.begin.offset : 0;
    const std::size_t end = index == range.end.paragraph ? range.end.offset
                                                         : paragraph.text.size();
    paragraph.text = JwpText(paragraph.text.begin() + static_cast<std::ptrdiff_t>(begin),
                             paragraph.text.begin() + static_cast<std::ptrdiff_t>(end));
    fragment.paragraphs.push_back(std::move(paragraph));
  }
  // The source clears first-line indentation on the first copied paragraph so
  // a fragment does not unexpectedly indent at a new insertion point.
  fragment.paragraphs.front().first_indent = 0;
  validate_fragment(fragment);
  return fragment;
}

JwpPosition paste_jwp_fragment(JwpDocumentModel& destination, JwpRange range,
                               const JwpDocument& fragment) {
  require_range(destination, range);
  validate_fragment(fragment);

  JwpDocumentModel candidate(destination.document());
  const JwpPosition insertion = candidate.erase(range);
  if (fragment.paragraphs.size() == 1) {
    const JwpPosition result = candidate.insert(insertion, fragment.paragraphs.front().text);
    destination = std::move(candidate);
    return result;
  }

  JwpDocument updated = candidate.document();
  JwpParagraph& target = updated.paragraphs[insertion.paragraph];
  JwpText suffix(target.text.begin() + static_cast<std::ptrdiff_t>(insertion.offset),
                 target.text.end());
  target.text.erase(target.text.begin() + static_cast<std::ptrdiff_t>(insertion.offset),
                    target.text.end());
  target.page_break = false;
  target.text.insert(target.text.end(), fragment.paragraphs.front().text.begin(),
                     fragment.paragraphs.front().text.end());

  std::vector<JwpParagraph> inserted;
  inserted.reserve(fragment.paragraphs.size() - 1);
  inserted.insert(inserted.end(), fragment.paragraphs.begin() + 1,
                  fragment.paragraphs.end());
  JwpParagraph& last = inserted.back();
  if (last.page_break && (!last.text.empty() || !suffix.empty()))
    throw JwpClipboardError("Clipboard page break cannot contain pasted text");
  const std::size_t caret_offset = last.text.size();
  last.text.insert(last.text.end(), suffix.begin(), suffix.end());
  updated.paragraphs.insert(
      updated.paragraphs.begin() + static_cast<std::ptrdiff_t>(insertion.paragraph + 1),
      inserted.begin(), inserted.end());

  try {
    (void)encode_jwp_document(updated);
  } catch (const JwpFormatError& error) {
    throw JwpClipboardError(error.what());
  }
  destination = JwpDocumentModel(std::move(updated));
  return {insertion.paragraph + fragment.paragraphs.size() - 1, caret_offset};
}

std::string encode_jwp_clipboard_fragment(const JwpClipboardFragment& fragment,
                                          std::size_t maximum_bytes) {
  if (maximum_bytes < kHeaderSize)
    throw JwpClipboardError("Clipboard byte limit is too small");
  validate_fragment(fragment.document);
  const auto code_page = static_cast<std::uint16_t>(fragment.code_page);
  if (!supported_code_page(code_page))
    throw JwpClipboardError("Clipboard code page is unsupported");

  std::string payload;
  try {
    payload = encode_jwp_document(fragment.document);
  } catch (const JwpFormatError& error) {
    throw JwpClipboardError(error.what());
  }
  if (payload.size() > std::numeric_limits<std::uint32_t>::max() ||
      payload.size() > maximum_bytes - kHeaderSize)
    throw JwpClipboardError("Clipboard fragment exceeds its byte limit");

  std::string output;
  output.reserve(kHeaderSize + payload.size());
  output.append(kMagic.data(), kMagic.size());
  write_u16(output, kVersion);
  write_u16(output, code_page);
  write_u32(output, static_cast<std::uint32_t>(payload.size()));
  output += payload;
  return output;
}

JwpClipboardFragment decode_jwp_clipboard_fragment(std::string_view bytes,
                                                   std::size_t maximum_bytes) {
  if (maximum_bytes < kHeaderSize || bytes.size() > maximum_bytes)
    throw JwpClipboardError("Clipboard fragment exceeds its byte limit");
  if (bytes.size() < kHeaderSize ||
      !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
    throw JwpClipboardError("Clipboard fragment has invalid magic");
  if (read_u16(bytes, 4) != kVersion)
    throw JwpClipboardError("Clipboard fragment version is unsupported");
  const std::uint16_t code_page = read_u16(bytes, 6);
  if (!supported_code_page(code_page))
    throw JwpClipboardError("Clipboard code page is unsupported");
  const std::size_t payload_size = read_u32(bytes, 8);
  if (payload_size != bytes.size() - kHeaderSize)
    throw JwpClipboardError("Clipboard fragment size is invalid");

  JwpDocument document;
  try {
    document = decode_jwp_document(bytes.substr(kHeaderSize));
  } catch (const JwpFormatError& error) {
    throw JwpClipboardError(error.what());
  }
  validate_fragment(document);
  return {std::move(document), static_cast<LegacyCodePage>(code_page)};
}

}  // namespace jwpqt::core
