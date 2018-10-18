#include "terminal.hh"

#include <sys/ioctl.h>
#include <unistd.h>

#include <sstream>
#include <string>

namespace terminal {

namespace {
int g_cached_columns = -1;
WantColor g_want_color = WantColor::AUTO;
}  // namespace

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
    c = isatty(STDOUT_FILENO) == 1 ? 80 : 0;
  }

  g_cached_columns = c;
  return g_cached_columns;
}

#define DEFINE_COLOR(ColorName, ColorCode)       \
  std::string ColorName(const std::string& s) {  \
    if (g_want_color == WantColor::NO) return s; \
    std::stringstream ss;                        \
    ss << (ColorCode) << s << "\033[0m";         \
    return ss.str();                             \
  }

DEFINE_COLOR(Bold, "\033[1m");
DEFINE_COLOR(BoldRed, "\033[1;31m");
DEFINE_COLOR(BoldGreen, "\033[1;32m");
DEFINE_COLOR(BoldMagenta, "\033[1;35m");
DEFINE_COLOR(BoldCyan, "\033[1;36m");

#undef DEFINE_COLOR

}  // namespace terminal
