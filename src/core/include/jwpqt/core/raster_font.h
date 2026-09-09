// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef JWPQT_CORE_RASTER_FONT_H
#define JWPQT_CORE_RASTER_FONT_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "jwpqt/core/jis_encoding.h"

namespace jwpqt::core {

bool jwp_glyph_rotates(JisCode code) noexcept;

class RasterFontError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class RasterFont {
 public:
  explicit RasterFont(std::string_view bytes);
  int width() const noexcept { return width_; }
  int height() const noexcept { return height_; }
  int leading() const noexcept { return leading_; }
  int spacing() const noexcept { return spacing_; }
  std::size_t glyph_count() const noexcept { return count_; }
  std::size_t glyph_index(JisCode code) const noexcept;
  bool ink(std::size_t glyph, int x, int y) const;
  // Source bitmap offsets after counter-rotation, in original pixel units.
  std::pair<int, int> vertical_offset(JisCode code) const;

  // Private, bounded outline face: original pixels become rectangles, not
  // smoothed guesses. Restricted embedding does not grant rights to the source.
  std::string native_face(std::string_view family) const;

 private:
  std::string data_;
  int width_ = 0, height_ = 0, leading_ = 0, spacing_ = 0;
  std::size_t stride_ = 0, glyph_bytes_ = 0, count_ = 0;
  bool holes_ = false;
};

}  // namespace jwpqt::core
#endif
