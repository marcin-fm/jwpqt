// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_WNN_SESSION_H
#define JWPQT_CORE_WNN_SESSION_H

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "jwpqt/core/wnn_dictionary.h"
#include "jwpqt/core/wnn_lookup.h"
#include "jwpqt/core/wnn_preferences.h"

namespace jwpqt::core {

class WnnSessionError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class WnnConversionSession {
 public:
  WnnConversionSession(
      const WnnDictionary& system_dictionary, WnnPreferences& preferences,
      const std::vector<WnnRecord>* user_records = nullptr,
      std::size_t maximum_output_cells = kWnnDefaultMaximumLookupCells);

  bool begin(const JwpText& input);
  void clear() noexcept;

  bool active() const noexcept;
  const JwpText& input() const;
  const WnnLookupResult& result() const;
  std::size_t selected_index() const;
  const WnnCandidate& selected_candidate() const;

  bool select(std::size_t candidate_index);
  bool cycle_next();
  bool cycle_previous();

  JwpText accept();
  JwpText cancel();

 private:
  void require_active() const;

  const WnnDictionary& system_dictionary_;
  WnnPreferences& preferences_;
  const std::vector<WnnRecord>* user_records_;
  std::size_t maximum_output_cells_;
  JwpText input_;
  WnnLookupResult result_;
  std::size_t selected_index_ = 0;
  bool active_ = false;
};

}  // namespace jwpqt::core

#endif
