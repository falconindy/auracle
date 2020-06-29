// SPDX-License-Identifier: MIT
#include "terminal.hh"

#include <sys/ioctl.h>
#include <unistd.h>

#include <string>

#include "absl/strings/str_cat.h"

namespace terminal {

namespace {
constexpr int kDefaultColumns = 80;
int g_cached_columns = -1;
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

int Columns() {
  int c;
  struct winsize ws {};

  if (g_cached_columns >= 0) {
    return g_cached_columns;
  }

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
    c = ws.ws_col;
  } else {
    c = isatty(STDOUT_FILENO) == 1 ? kDefaultColumns : 0;
  }

  g_cached_columns = c;
  return g_cached_columns;
}

}  // namespace terminal
