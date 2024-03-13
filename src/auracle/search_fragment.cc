// SPDX-License-Identifier: MIT
#include "search_fragment.hh"

#include <string.h>

#include <vector>

namespace auracle {

// We know that the AUR will reject search strings shorter than 2 characters.
constexpr std::string_view::size_type kMinCandidateSize = 2;

constexpr std::string_view regex_chars = R"(^.+*?$[](){}|\)";

std::string_view GetSearchFragment(std::string_view input) {
  std::vector<std::string_view> candidates;

  while (input.size() >= 2) {
    if (input.front() == '[' || input.front() == '{') {
      auto brace_end = input.find_first_of("]}");
      if (brace_end == input.npos) {
        // This may or may not be an invalid regex, e.g. we might have
        // "foo\[bar". In practice, this should never happen because package
        // names shouldn't have such characters.
        return std::string_view();
      }

      input.remove_prefix(brace_end);
      continue;
    }

    auto span = input.find_first_of(regex_chars);
    if (span == input.npos) {
      span = input.size();
    } else if (span == 0) {
      input.remove_prefix(1);
      continue;
    }

    // given 'cow?', we can't include w in the search
    // see if a ? or * follows cand
    auto cand = input.substr(0, span);
    if (input.size() > span && (input[span] == '?' || input[span] == '*')) {
      cand.remove_suffix(1);
    }

    if (cand.size() < kMinCandidateSize) {
      input.remove_prefix(1);
      continue;
    }

    candidates.push_back(cand);
    input.remove_prefix(span);
  }

  std::string_view longest;
  for (auto cand : candidates) {
    if (cand.size() > longest.size()) {
      longest = cand;
    }
  }

  return longest;
}

}  // namespace auracle
