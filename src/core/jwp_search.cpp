#include "jwpqt/core/jwp_search.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace jwpqt::core {

namespace {

JisCode normalize_token(JisCode token, const JwpSearchOptions& options) {
  if (token >= 0x2400) {
    return token;
  }

  if (options.jascii_ascii_equivalence && (token & 0x7f00U) == 0x2300U) {
    if (token >= 0x2330 && token <= 0x2339) {
      token = static_cast<JisCode>('0' + (token - 0x2330));
    } else if (token >= 0x2341 && token <= 0x235a) {
      token = static_cast<JisCode>('A' + (token - 0x2341));
    } else if (token >= 0x2361 && token <= 0x237a) {
      token = static_cast<JisCode>('a' + (token - 0x2361));
    } else {
      token = 0;
    }
  }

  if (options.ignore_ascii_case && token >= 'A' && token <= 'Z') {
    token = static_cast<JisCode>('a' + (token - 'A'));
  }
  return token;
}

bool matches_at(const JwpText& text, std::size_t offset,
                const JwpText& pattern, const JwpSearchOptions& options) {
  for (std::size_t index = 0; index < pattern.size(); ++index) {
    if (normalize_token(text[offset + index], options) !=
        normalize_token(pattern[index], options)) {
      return false;
    }
  }
  return true;
}

std::optional<JwpRange> find_forward_in_paragraph(
    const JwpText& text, const JwpText& pattern, std::size_t begin,
    std::size_t end, std::size_t paragraph,
    const JwpSearchOptions& options) {
  if (pattern.size() > text.size()) {
    return std::nullopt;
  }
  const std::size_t last = text.size() - pattern.size();
  const std::size_t bounded_end = std::min(end, last + 1);
  for (std::size_t offset = begin; offset < bounded_end; ++offset) {
    if (matches_at(text, offset, pattern, options)) {
      return JwpRange{{paragraph, offset},
                      {paragraph, offset + pattern.size()}};
    }
  }
  return std::nullopt;
}

std::optional<JwpRange> find_backward_in_paragraph(
    const JwpText& text, const JwpText& pattern, std::size_t first,
    std::size_t lower_bound, std::size_t paragraph,
    const JwpSearchOptions& options) {
  if (pattern.size() > text.size() || first < lower_bound) {
    return std::nullopt;
  }
  std::size_t offset = std::min(first, text.size() - pattern.size());
  if (offset < lower_bound) {
    return std::nullopt;
  }
  while (true) {
    if (matches_at(text, offset, pattern, options)) {
      return JwpRange{{paragraph, offset},
                      {paragraph, offset + pattern.size()}};
    }
    if (offset == lower_bound) {
      break;
    }
    --offset;
  }
  return std::nullopt;
}

JwpSearchResult find_forward(const JwpDocumentModel& model,
                             const JwpText& pattern, JwpPosition start,
                             const JwpSearchOptions& options) {
  for (std::size_t paragraph = start.paragraph;
       paragraph < model.paragraph_count(); ++paragraph) {
    const JwpText& text = model.paragraph(paragraph).text;
    std::size_t begin = 0;
    if (paragraph == start.paragraph) {
      if (start.offset == text.size()) {
        continue;
      }
      begin = start.offset + 1;
    }
    if (auto match = find_forward_in_paragraph(
            text, pattern, begin, text.size(), paragraph, options)) {
      return {match, false, false};
    }
  }

  if (!options.wrap) {
    return {};
  }
  for (std::size_t paragraph = 0;; ++paragraph) {
    const JwpText& text = model.paragraph(paragraph).text;
    const std::size_t end =
        paragraph == start.paragraph ? start.offset : text.size();
    if (auto match =
            find_forward_in_paragraph(text, pattern, 0, end, paragraph,
                                      options)) {
      return {match, true, false};
    }
    if (paragraph == start.paragraph) {
      break;
    }
  }
  return {};
}

JwpSearchResult find_backward(const JwpDocumentModel& model,
                              const JwpText& pattern, JwpPosition start,
                              const JwpSearchOptions& options) {
  std::size_t paragraph = start.paragraph;
  while (true) {
    const JwpText& text = model.paragraph(paragraph).text;
    if (paragraph != start.paragraph || start.offset != 0) {
      const std::size_t first = paragraph == start.paragraph
                                    ? start.offset - 1
                                    : text.size();
      if (auto match = find_backward_in_paragraph(text, pattern, first, 0,
                                                   paragraph, options)) {
        return {match, false, false};
      }
    }
    if (paragraph == 0) {
      break;
    }
    --paragraph;
  }

  if (!options.wrap) {
    return {};
  }
  paragraph = model.paragraph_count() - 1;
  while (paragraph >= start.paragraph) {
    const JwpText& text = model.paragraph(paragraph).text;
    if (paragraph != start.paragraph || start.offset != text.size()) {
      const std::size_t lower_bound =
          paragraph == start.paragraph ? start.offset + 1 : 0;
      if (auto match = find_backward_in_paragraph(
              text, pattern, text.size(), lower_bound, paragraph, options)) {
        return {match, true, false};
      }
    }
    if (paragraph == start.paragraph) {
      break;
    }
    --paragraph;
  }
  return {};
}

}  // namespace

JwpSearchResult find_next(const JwpDocumentModel& model,
                          const JwpText& pattern, JwpPosition start,
                          JwpSearchOptions options) {
  if (!model.valid_position(start)) {
    throw JwpSearchError("search start is out of range");
  }
  if (pattern.empty()) {
    return {std::nullopt, false, true};
  }
  if (options.direction == JwpSearchDirection::kBackward) {
    return find_backward(model, pattern, start, options);
  }
  return find_forward(model, pattern, start, options);
}

JwpPosition replace_range(JwpDocumentModel& model, JwpRange range,
                          const JwpText& replacement) {
  JwpDocumentModel updated(model.document());
  const JwpPosition insertion = updated.erase(range);
  const JwpPosition caret = updated.insert(insertion, replacement);
  model = std::move(updated);
  return caret;
}

}  // namespace jwpqt::core
