#include "jwpqt/core/jwp_document_model.h"

#include <cstdint>
#include <utility>

namespace jwpqt::core {
namespace {

JwpDocument ensure_paragraph(JwpDocument document) {
  if (document.paragraphs.empty()) {
    document.paragraphs.emplace_back();
  }
  return document;
}

}  // namespace

JwpDocumentModel::JwpDocumentModel() : document_(ensure_paragraph({})) {}

JwpDocumentModel::JwpDocumentModel(JwpDocument document)
    : document_(ensure_paragraph(std::move(document))) {}

const JwpDocument& JwpDocumentModel::document() const noexcept {
  return document_;
}

std::size_t JwpDocumentModel::paragraph_count() const noexcept {
  return document_.paragraphs.size();
}

const JwpParagraph& JwpDocumentModel::paragraph(std::size_t index) const {
  if (index >= paragraph_count()) {
    throw JwpDocumentEditError("paragraph index is out of range");
  }
  return document_.paragraphs[index];
}

JwpParagraphFormat JwpDocumentModel::paragraph_format(
    std::size_t index) const {
  const JwpParagraph& source = paragraph(index);
  return {source.left_indent, source.right_indent, source.first_indent,
          source.line_spacing};
}

bool JwpDocumentModel::valid_position(JwpPosition position) const noexcept {
  return position.paragraph < paragraph_count() &&
         position.offset <= document_.paragraphs[position.paragraph].text.size();
}

JwpPosition JwpDocumentModel::insert(JwpPosition position,
                                     const JwpText& text) {
  require_position(position);
  auto& target = document_.paragraphs[position.paragraph];
  if (target.page_break && !text.empty()) {
    throw JwpDocumentEditError("cannot insert text into a page break");
  }

  const JwpText inserted(text.begin(), text.end());
  target.text.insert(target.text.begin() +
                         static_cast<JwpText::difference_type>(position.offset),
                     inserted.begin(), inserted.end());
  position.offset += inserted.size();
  return position;
}

JwpPosition JwpDocumentModel::erase(JwpRange range) {
  require_position(range.begin);
  require_position(range.end);
  if (range.end < range.begin) {
    throw JwpDocumentEditError("document range is reversed");
  }
  if (range.begin == range.end) {
    return range.begin;
  }

  auto& paragraphs = document_.paragraphs;
  if (range.begin.paragraph == range.end.paragraph) {
    auto& text = paragraphs[range.begin.paragraph].text;
    text.erase(text.begin() +
                   static_cast<JwpText::difference_type>(range.begin.offset),
               text.begin() +
                   static_cast<JwpText::difference_type>(range.end.offset));
    return range.begin;
  }

  const auto& first = paragraphs[range.begin.paragraph];
  const auto& last = paragraphs[range.end.paragraph];
  JwpText merged;
  merged.reserve(range.begin.offset + last.text.size() - range.end.offset);
  merged.insert(merged.end(), first.text.begin(),
                first.text.begin() + static_cast<JwpText::difference_type>(
                                         range.begin.offset));
  merged.insert(merged.end(),
                last.text.begin() +
                    static_cast<JwpText::difference_type>(range.end.offset),
                last.text.end());

  paragraphs[range.begin.paragraph].text = std::move(merged);
  paragraphs[range.begin.paragraph].page_break = false;
  paragraphs.erase(
      paragraphs.begin() + static_cast<std::ptrdiff_t>(range.begin.paragraph + 1),
      paragraphs.begin() + static_cast<std::ptrdiff_t>(range.end.paragraph + 1));
  return range.begin;
}

JwpPosition JwpDocumentModel::split_paragraph(JwpPosition position) {
  require_position(position);
  auto& paragraphs = document_.paragraphs;
  JwpParagraph next = paragraphs[position.paragraph];

  if (next.page_break) {
    if (!next.text.empty() || position.offset != 0) {
      throw JwpDocumentEditError("page break contains text");
    }
    paragraphs.insert(
        paragraphs.begin() + static_cast<std::ptrdiff_t>(position.paragraph + 1),
        std::move(next));
    paragraphs[position.paragraph].page_break = false;
    return {position.paragraph + 1, 0};
  }

  auto& current = paragraphs[position.paragraph];
  next.text.assign(
      current.text.begin() +
          static_cast<JwpText::difference_type>(position.offset),
      current.text.end());
  next.page_break = false;
  paragraphs.insert(
      paragraphs.begin() + static_cast<std::ptrdiff_t>(position.paragraph + 1),
      std::move(next));
  paragraphs[position.paragraph].text.erase(
      paragraphs[position.paragraph].text.begin() +
          static_cast<JwpText::difference_type>(position.offset),
      paragraphs[position.paragraph].text.end());
  return {position.paragraph + 1, 0};
}

JwpPosition JwpDocumentModel::join_with_next(std::size_t paragraph_index) {
  auto& paragraphs = document_.paragraphs;
  if (paragraph_index >= paragraphs.size() ||
      paragraph_index == paragraphs.size() - 1) {
    throw JwpDocumentEditError("paragraph has no successor");
  }

  auto& current = paragraphs[paragraph_index];
  const auto& next = paragraphs[paragraph_index + 1];
  if (current.text.empty() && next.page_break) {
    paragraphs.erase(paragraphs.begin() +
                     static_cast<std::ptrdiff_t>(paragraph_index));
    return {paragraph_index, 0};
  }

  const std::size_t join_offset = current.text.size();
  JwpText joined = current.text;
  joined.insert(joined.end(), next.text.begin(), next.text.end());
  current.text = std::move(joined);
  current.page_break = false;
  paragraphs.erase(paragraphs.begin() +
                   static_cast<std::ptrdiff_t>(paragraph_index + 1));
  return {paragraph_index, join_offset};
}

JwpPosition JwpDocumentModel::insert_page_break(JwpPosition position) {
  require_position(position);
  const JwpParagraph source = document_.paragraphs[position.paragraph];
  if (source.page_break && !source.text.empty()) {
    throw JwpDocumentEditError("page break contains text");
  }

  JwpDocument updated = document_;
  auto& paragraphs = updated.paragraphs;
  const auto after = paragraphs.begin() +
                     static_cast<std::ptrdiff_t>(position.paragraph + 1);

  if (source.page_break) {
    paragraphs.insert(after, source);
    document_ = std::move(updated);
    return {position.paragraph + 1, 0};
  }

  if (source.text.empty()) {
    paragraphs[position.paragraph].page_break = true;
    if (position.paragraph + 1 == paragraphs.size()) {
      paragraphs.push_back(source);
    }
    document_ = std::move(updated);
    return {position.paragraph + 1, 0};
  }

  JwpParagraph page_break = source;
  page_break.text.clear();
  page_break.page_break = true;

  if (position.offset == 0) {
    paragraphs[position.paragraph] = page_break;
    paragraphs.insert(after, source);
    document_ = std::move(updated);
    return {position.paragraph + 1, 0};
  }

  if (position.offset == source.text.size()) {
    const bool had_successor = position.paragraph + 1 < paragraphs.size();
    paragraphs.insert(after, page_break);
    if (!had_successor) {
      JwpParagraph trailing = source;
      trailing.text.clear();
      paragraphs.push_back(std::move(trailing));
    }
    document_ = std::move(updated);
    return {position.paragraph + 2, 0};
  }

  JwpParagraph suffix = source;
  suffix.text.erase(
      suffix.text.begin(),
      suffix.text.begin() + static_cast<JwpText::difference_type>(position.offset));
  paragraphs[position.paragraph].text.erase(
      paragraphs[position.paragraph].text.begin() +
          static_cast<JwpText::difference_type>(position.offset),
      paragraphs[position.paragraph].text.end());
  paragraphs.insert(after, page_break);
  paragraphs.insert(paragraphs.begin() +
                        static_cast<std::ptrdiff_t>(position.paragraph + 2),
                    std::move(suffix));
  document_ = std::move(updated);
  return {position.paragraph + 2, 0};
}

void JwpDocumentModel::set_page_break(std::size_t paragraph_index,
                                      bool page_break) {
  if (paragraph_index >= paragraph_count()) {
    throw JwpDocumentEditError("paragraph index is out of range");
  }
  auto& target = document_.paragraphs[paragraph_index];
  target.page_break = page_break;
  target.text.clear();
}

void JwpDocumentModel::format_paragraphs(
    std::size_t first_paragraph, std::size_t last_paragraph,
    const JwpParagraphFormat& format) {
  if (first_paragraph > last_paragraph) {
    throw JwpDocumentEditError("paragraph range is reversed");
  }
  if (last_paragraph >= paragraph_count()) {
    throw JwpDocumentEditError("paragraph index is out of range");
  }
  if (format.left_indent < 0 || format.left_indent > 255 ||
      format.right_indent < 0 || format.right_indent > 255) {
    throw JwpDocumentEditError("paragraph side indent is out of range");
  }
  if (format.first_indent < -127 || format.first_indent > 127) {
    throw JwpDocumentEditError("paragraph first indent is out of range");
  }
  if (format.first_indent < -format.left_indent) {
    throw JwpDocumentEditError(
        "paragraph first indent extends beyond the left margin");
  }
  if (format.line_spacing < 100 || format.line_spacing > 1000) {
    throw JwpDocumentEditError("paragraph line spacing is out of range");
  }

  for (std::size_t index = first_paragraph; index <= last_paragraph; ++index) {
    JwpParagraph& paragraph = document_.paragraphs[index];
    paragraph.left_indent = static_cast<std::uint8_t>(format.left_indent);
    paragraph.right_indent = static_cast<std::uint8_t>(format.right_indent);
    paragraph.first_indent = static_cast<std::int8_t>(format.first_indent);
    paragraph.line_spacing =
        static_cast<std::int16_t>(format.line_spacing);
  }
}

void JwpDocumentModel::require_position(JwpPosition position) const {
  if (!valid_position(position)) {
    throw JwpDocumentEditError("document position is out of range");
  }
}

}  // namespace jwpqt::core
