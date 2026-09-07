// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace jwpqt::qt {

struct EdictLookupOptions {
  bool personal_names = false;
  bool place_names = false;
  bool classical = false;
  bool require_beginning = true;
  bool require_end = false;
  bool advanced = false;
  bool advanced_always = true;
  bool advanced_show_all = false;
  bool i_adjectives = true;
  bool full_ascii = false;
  bool jascii_to_ascii = false;
};

}  // namespace jwpqt::qt
