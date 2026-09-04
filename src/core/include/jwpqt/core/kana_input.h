// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JWPQT_CORE_KANA_INPUT_H
#define JWPQT_CORE_KANA_INPUT_H

#include <stdexcept>
#include <vector>

#include "jwpqt/core/jwp_document.h"

namespace jwpqt::core {

enum class KanaInputKind {
  kKanaStart,
  kKanaContinue,
  kText,
};

struct KanaInputEvent {
  KanaInputKind kind = KanaInputKind::kText;
  JwpText text;

  bool operator==(const KanaInputEvent& other) const noexcept;
};

struct KanaInputOptions {
  bool old_katakana_input = false;
};

class KanaInputError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class KanaInputComposer {
 public:
  explicit KanaInputComposer(KanaInputOptions options = {});

  // Consumes one printable 7-bit ASCII key and returns completed output.
  std::vector<KanaInputEvent> push_ascii(char input);
  // Resolves buffered input. Incomplete input is retained and reported.
  std::vector<KanaInputEvent> flush();
  // Drops buffered input and reports whether anything was dropped.
  bool discard() noexcept;

  bool pending() const noexcept;
  // Matches the legacy AIUEONn ambiguity check.
  bool pending_ambiguous() const noexcept;
  // Changing behavior in the middle of a composition is rejected.
  void set_options(KanaInputOptions options);

 private:
  bool emit_buffer(std::vector<KanaInputEvent>& events) const;
  void clear_state() noexcept;
  void flush_pending(std::vector<KanaInputEvent>& events);

  KanaInputOptions options_;
  std::string buffer_;
  std::size_t state_ = 0;
  bool pending_output_ = false;
  bool kana_start_ = false;
};

}  // namespace jwpqt::core

#endif
