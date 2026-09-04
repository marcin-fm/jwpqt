// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_JIS_ENCODING_H
#define JWPQT_CORE_JIS_ENCODING_H

#include <cstdint>
#include <stdexcept>

namespace jwpqt::core {

using JisCode = std::uint16_t;

struct EncodedPair {
  std::uint8_t lead;
  std::uint8_t trail;
};

class JisEncodingError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Checks the two-byte 94x94 code-space shape, not character assignment.
bool is_jis_x0208_pair(JisCode code) noexcept;

EncodedPair encode_euc_jp_pair(JisCode code);
JisCode decode_euc_jp_pair(EncodedPair bytes);

EncodedPair encode_shift_jis_pair(JisCode code);
JisCode decode_shift_jis_pair(EncodedPair bytes);

}  // namespace jwpqt::core

#endif
