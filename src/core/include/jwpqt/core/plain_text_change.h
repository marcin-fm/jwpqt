#ifndef JWPQT_CORE_PLAIN_TEXT_CHANGE_H
#define JWPQT_CORE_PLAIN_TEXT_CHANGE_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace jwpqt::core {

class PlainTextChangeError : public std::runtime_error {
 public:
  explicit PlainTextChangeError(const std::string& message);
};

std::u32string replace_plain_text_snapshot(
    std::u32string_view previous, std::u32string_view current,
    std::size_t offset, std::size_t removed_length,
    std::u32string_view replacement);

}  // namespace jwpqt::core

#endif
