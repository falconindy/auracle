#include "search_fragment.hh"

#include <string.h>

#include <vector>

namespace auracle {

std::string_view GetSearchFragment(std::string_view input) {
  constexpr char kRegexChars[] = "^.+*?$[](){}|\\";

  std::vector<std::string_view> candidates;
  for (const char* argstr = input.data(); *argstr != '\0'; argstr++) {
    int span = strcspn(argstr, kRegexChars);

    // given 'cow?', we can't include w in the search
    if (argstr[span] == '?' || argstr[span] == '*') {
      span--;
    }

    // a string inside [] or {} cannot be a valid span
    if (strchr("[{", *argstr) != nullptr) {
      argstr = strpbrk(argstr + span, "]}");
      if (argstr == nullptr) {
        // This may or may not be an invalid regex, e.g. we might have
        // "foo\[bar". In practice, this should never happen because package
        // names shouldn't have such characters.
        return std::string_view();
      }
      continue;
    }

    if (span >= 2) {
      candidates.emplace_back(argstr, span);
      argstr += span - 1;
      continue;
    }
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
