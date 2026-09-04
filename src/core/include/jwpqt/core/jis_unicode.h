// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_JIS_UNICODE_H
#define JWPQT_CORE_JIS_UNICODE_H

#include <optional>

#include "jwpqt/core/jis_encoding.h"

namespace jwpqt::core {

// Maps JWP's JIS X 0208 plane representation. Structurally valid but
// unassigned pairs have no mapping.
std::optional<char32_t> jis_x0208_to_unicode(JisCode code) noexcept;
std::optional<JisCode> unicode_to_jis_x0208(char32_t code_point) noexcept;

}  // namespace jwpqt::core

#endif
