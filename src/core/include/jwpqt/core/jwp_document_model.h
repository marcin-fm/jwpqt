#pragma once

#include "jwpqt/core/jwp_document.h"

#include <cstddef>
#include <stdexcept>

namespace jwpqt::core {

struct JwpPosition {
  std::size_t paragraph = 0;
  std::size_t offset = 0;

  bool operator==(const JwpPosition& other) const noexcept {
    return paragraph == other.paragraph && offset == other.offset;
  }

  bool operator!=(const JwpPosition& other) const noexcept {
    return !(*this == other);
  }

  bool operator<(const JwpPosition& other) const noexcept {
    return paragraph < other.paragraph ||
           (paragraph == other.paragraph && offset < other.offset);
  }
};

struct JwpRange {
  JwpPosition begin;
  JwpPosition end;
};

class JwpDocumentEditError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class JwpDocumentModel {
 public:
  JwpDocumentModel();
  explicit JwpDocumentModel(JwpDocument document);

  const JwpDocument& document() const noexcept;
  std::size_t paragraph_count() const noexcept;
  const JwpParagraph& paragraph(std::size_t index) const;
  bool valid_position(JwpPosition position) const noexcept;

  JwpPosition insert(JwpPosition position, const JwpText& text);
  JwpPosition erase(JwpRange range);
  JwpPosition split_paragraph(JwpPosition position);
  JwpPosition join_with_next(std::size_t paragraph);
  void set_page_break(std::size_t paragraph, bool page_break);

  // Mutating operations can invalidate references returned by paragraph().

 private:
  void require_position(JwpPosition position) const;

  JwpDocument document_;
};

}  // namespace jwpqt::core
