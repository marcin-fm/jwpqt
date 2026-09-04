// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/jis_encoding.h"

// Shift-JIS pair formulas derive from Ken R. Lunde's jconv.c 3.0
// (1993-07-01), as attributed and preserved in legacy jwp_jisc.cpp.

namespace jwpqt::core {
namespace {

constexpr unsigned int kJisFirst = 0x21;
constexpr unsigned int kJisLast = 0x7e;

unsigned int high_byte(JisCode code) {
  return static_cast<unsigned int>((code >> 8U) & 0xffU);
}

unsigned int low_byte(JisCode code) {
  return static_cast<unsigned int>(code & 0xffU);
}

JisCode make_jis_code(unsigned int row, unsigned int cell) {
  return static_cast<JisCode>((row << 8U) | cell);
}

void require_jis_x0208(JisCode code) {
  if (!is_jis_x0208_pair(code)) {
    throw JisEncodingError("Invalid JIS X 0208 code");
  }
}

}  // namespace

bool is_jis_x0208_pair(JisCode code) noexcept {
  const unsigned int row = high_byte(code);
  const unsigned int cell = low_byte(code);
  return row >= kJisFirst && row <= kJisLast && cell >= kJisFirst &&
         cell <= kJisLast;
}

EncodedPair encode_euc_jp_pair(JisCode code) {
  require_jis_x0208(code);
  return {
      static_cast<std::uint8_t>(high_byte(code) | 0x80U),
      static_cast<std::uint8_t>(low_byte(code) | 0x80U),
  };
}

JisCode decode_euc_jp_pair(EncodedPair bytes) {
  const unsigned int lead = bytes.lead;
  const unsigned int trail = bytes.trail;
  if (lead < 0xa1U || lead > 0xfeU || trail < 0xa1U || trail > 0xfeU) {
    throw JisEncodingError("Invalid two-byte EUC-JP sequence");
  }
  return make_jis_code(lead & 0x7fU, trail & 0x7fU);
}

EncodedPair encode_shift_jis_pair(JisCode code) {
  require_jis_x0208(code);

  const unsigned int row = high_byte(code);
  const unsigned int cell = low_byte(code);
  const unsigned int trail =
      cell + ((row % 2U != 0U) ? ((cell > 0x5fU) ? 0x20U : 0x1fU)
                               : 0x7eU);
  const unsigned int lead =
      ((row + 1U) >> 1U) + ((row < 0x5fU) ? 0x70U : 0xb0U);

  return {
      static_cast<std::uint8_t>(lead),
      static_cast<std::uint8_t>(trail),
  };
}

JisCode decode_shift_jis_pair(EncodedPair bytes) {
  const unsigned int lead = bytes.lead;
  const unsigned int trail = bytes.trail;
  const bool valid_lead =
      (lead >= 0x81U && lead <= 0x9fU) ||
      (lead >= 0xe0U && lead <= 0xefU);
  const bool valid_trail =
      (trail >= 0x40U && trail <= 0x7eU) ||
      (trail >= 0x80U && trail <= 0xfcU);
  if (!valid_lead || !valid_trail) {
    throw JisEncodingError("Invalid two-byte Shift-JIS sequence");
  }

  const bool adjust = trail < 0x9fU;
  const unsigned int row_offset = lead < 0xa0U ? 0x70U : 0xb0U;
  const unsigned int cell_offset =
      adjust ? ((trail > 0x7fU) ? 0x20U : 0x1fU) : 0x7eU;
  const unsigned int row = ((lead - row_offset) << 1U) - (adjust ? 1U : 0U);
  const unsigned int cell = trail - cell_offset;
  const JisCode code = make_jis_code(row, cell);
  require_jis_x0208(code);
  return code;
}

}  // namespace jwpqt::core
