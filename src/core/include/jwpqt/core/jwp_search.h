#pragma once

#include "jwpqt/core/jwp_document_model.h"

#include <optional>
#include <stdexcept>

namespace jwpqt::core {

enum class JwpSearchDirection {
  kForward,
  kBackward,
};

struct JwpSearchOptions {
  JwpSearchDirection direction = JwpSearchDirection::kForward;
  bool ignore_ascii_case = true;
  // Mirrors JWPxp: only 0x23xx alphanumerics map to ASCII; other 0x23xx
  // tokens collapse to zero during comparison.
  bool jascii_ascii_equivalence = true;
  bool wrap = false;
};

struct JwpSearchResult {
  std::optional<JwpRange> match;
  bool wrapped = false;
  bool empty_pattern = false;
};

class JwpSearchError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

JwpSearchResult find_next(const JwpDocumentModel& model,
                          const JwpText& pattern, JwpPosition start,
                          JwpSearchOptions options = {});

JwpPosition replace_range(JwpDocumentModel& model, JwpRange range,
                          const JwpText& replacement);

}  // namespace jwpqt::core
