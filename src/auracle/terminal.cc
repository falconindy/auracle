// SPDX-License-Identifier: MIT
#include "terminal.hh"

#include <unistd.h>

#include <string>

#include "absl/strings/str_cat.h"

namespace terminal {

namespace {
WantColor g_want_color = WantColor::AUTO;

std::string Color(const std::string& s, const char* color) {
  if (g_want_color == WantColor::NO) {
    return s;
  }

  return absl::StrCat(color, s, "\033[0m");
}

}  // namespace

std::string Bold(const std::string& s) { return Color(s, "\033[1m"); }
std::string BoldRed(const std::string& s) { return Color(s, "\033[1;31m"); }
std::string BoldCyan(const std::string& s) { return Color(s, "\033[1;36m"); }
std::string BoldGreen(const std::string& s) { return Color(s, "\033[1;32m"); }
std::string BoldMagenta(const std::string& s) { return Color(s, "\033[1;35m"); }

void Init(WantColor want) {
  if (want == WantColor::AUTO) {
    g_want_color = isatty(STDOUT_FILENO) == 1 ? WantColor::YES : WantColor::NO;
  } else {
    g_want_color = want;
  }
}

}  // namespace terminal
