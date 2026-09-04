#include "jwpqt/core/plain_text_change.h"

namespace jwpqt::core {

PlainTextChangeError::PlainTextChangeError(const std::string& message)
    : std::runtime_error(message) {}

std::u32string replace_plain_text_snapshot(
    std::u32string_view previous, std::u32string_view current,
    std::size_t offset, std::size_t removed_length,
    std::u32string_view replacement) {
  if (offset > previous.size() ||
      removed_length > previous.size() - offset) {
    throw PlainTextChangeError("Plain-text change range is out of bounds");
  }

  std::size_t prefix = 0;
  while (prefix < previous.size() && prefix < current.size() &&
         previous[prefix] == current[prefix]) {
    ++prefix;
  }
  std::size_t suffix = 0;
  while (suffix < previous.size() - prefix &&
         suffix < current.size() - prefix &&
         previous[previous.size() - suffix - 1] ==
             current[current.size() - suffix - 1]) {
    ++suffix;
  }

  std::u32string expected(previous);
  expected.replace(offset, removed_length, replacement);
  if (expected != current) {
    throw PlainTextChangeError(
        "Plain-text change does not match the current snapshot");
  }

  std::u32string result(previous);
  result.replace(prefix, previous.size() - prefix - suffix,
                 current.substr(prefix, current.size() - prefix - suffix));
  return result;
}

}  // namespace jwpqt::core
