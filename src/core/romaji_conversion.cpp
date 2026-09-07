// SPDX-License-Identifier: GPL-2.0-or-later

#include "jwpqt/core/romaji_conversion.h"

#include <optional>
#include <utility>

#include "jwpqt/core/wnn_session.h"

namespace jwpqt::core {

JwpText convert_romaji_text(std::string_view input,
                            const WnnConversionSession* session,
                            KanaInputOptions options,
                            std::size_t maximum_cells) {
  if (maximum_cells > 65535 || input.empty() || input.size() > maximum_cells) {
    throw KanaInputError("romaji selection is empty or exceeds its size limit");
  }
  for (const char character : input) {
    const auto value = static_cast<unsigned char>(character);
    if (value != '\t' && (value < 0x20U || value >= 0x7fU)) {
      throw KanaInputError("romaji selection must contain printable ASCII or tabs");
    }
  }
  options.reject_incomplete = true;
  KanaInputComposer composer(options);
  JwpText output;
  std::optional<std::pair<std::size_t, std::size_t>> automatic_range;
  using Offset = JwpText::difference_type;

  const auto finish_automatic = [&](bool force) {
    if (!automatic_range) return;
    if (session != nullptr) {
      const auto [begin, end] = *automatic_range;
      const JwpText reading(output.begin() + static_cast<Offset>(begin),
                            output.begin() + static_cast<Offset>(end));
      auto prepared = session->prepare_automatic(reading, force);
      if (prepared.wait_for_more && !force) return;
      if (prepared.conversion) {
        if (prepared.matched_length == 0 || prepared.matched_length > end - begin) {
          throw KanaInputError("romaji conversion returned an invalid matched range");
        }
        const auto& candidate = prepared.conversion->selected_candidate().text;
        if (candidate.size() > maximum_cells - (output.size() - prepared.matched_length)) {
          throw KanaInputError("romaji conversion output exceeds its size limit");
        }
        const auto position = output.erase(
            output.begin() + static_cast<Offset>(begin),
            output.begin() + static_cast<Offset>(begin + prepared.matched_length));
        output.insert(position, candidate.begin(), candidate.end());
      }
    }
    automatic_range.reset();
  };

  const auto append_events = [&](const std::vector<KanaInputEvent>& events) {
    if (events.empty()) return;
    bool force = false;
    for (const auto& event : events) {
      if (event.text.empty()) continue;
      if (automatic_range && (event.kind == KanaInputKind::kKanaStart ||
                              output.size() != automatic_range->second)) {
        finish_automatic(true);
      }
      const std::size_t begin = output.size();
      if (event.text.size() > maximum_cells - output.size()) {
        throw KanaInputError("romaji conversion output exceeds its size limit");
      }
      output.insert(output.end(), event.text.begin(), event.text.end());
      if (event.kind == KanaInputKind::kKanaStart) {
        automatic_range = std::make_pair(begin, output.size());
        force = false;
      } else if (automatic_range && event.kind == KanaInputKind::kKanaContinue) {
        automatic_range->second = output.size();
      } else if (automatic_range && event.kind == KanaInputKind::kText) {
        force = true;
      }
    }
    finish_automatic(force);
  };

  for (const char value : input) {
    if (value == '\t') {
      append_events(composer.flush());
      append_events({{KanaInputKind::kText, JwpText{'\t'}}});
    } else {
      append_events(composer.push_ascii(value));
    }
  }
  if (automatic_range) append_events(composer.force_conversion());
  append_events(composer.flush());
  finish_automatic(true);
  return output;
}

}  // namespace jwpqt::core
