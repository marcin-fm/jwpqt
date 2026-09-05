// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_JWP_CONVERSION_H
#define JWPQT_CORE_JWP_CONVERSION_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include "jwpqt/core/jwp_document_history.h"
#include "jwpqt/core/wnn_session.h"

namespace jwpqt::core {

class JwpConversionError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class JwpConversionTransaction {
 public:
  JwpConversionTransaction(JwpDocumentModel& model,
                           JwpDocumentHistory& history,
                           WnnConversionSession& session);
  ~JwpConversionTransaction() noexcept;

  JwpConversionTransaction(const JwpConversionTransaction&) = delete;
  JwpConversionTransaction& operator=(const JwpConversionTransaction&) =
      delete;
  JwpConversionTransaction(JwpConversionTransaction&&) = delete;
  JwpConversionTransaction& operator=(JwpConversionTransaction&&) = delete;

  bool begin(JwpRange range, JwpPosition caret);
  void begin_prepared(JwpRange range, JwpPosition caret,
                      WnnPreparedConversion prepared);
  bool active() const noexcept;
  JwpRange range() const;
  JwpPosition caret() const;
  std::size_t selected_index() const;
  const WnnLookupResult& result() const;

  bool select(std::size_t candidate_index);
  bool cycle_next();
  bool cycle_previous();
  bool accept();
  JwpPosition rollback();

 private:
  void require_active() const;
  void require_expected_document() const;
  void start_prepared(JwpRange range, JwpPosition caret,
                      WnnPreparedConversion prepared);
  JwpPosition adjusted_caret(JwpPosition begin, JwpPosition end) const;
  bool replace_with(std::size_t candidate_index);
  void rollback_noexcept() noexcept;
  void finish() noexcept;

  JwpDocumentModel& model_;
  JwpDocumentHistory& history_;
  WnnConversionSession& session_;
  std::optional<JwpDocument> original_document_;
  std::optional<JwpDocument> expected_document_;
  JwpPosition original_caret_;
  JwpRange range_;
  JwpPosition caret_;
  std::uint64_t expected_history_generation_ = 0;
  std::uint64_t expected_session_generation_ = 0;
  std::size_t caret_tail_offset_ = 0;
  bool caret_at_start_ = false;
  bool active_ = false;
};

}  // namespace jwpqt::core

#endif
