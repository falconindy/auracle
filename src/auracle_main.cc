// SPDX-License-Identifier: MIT
#include <getopt.h>

#include <clocale>
#include <iostream>

#include "absl/container/flat_hash_map.h"
#include "auracle/auracle.hh"
#include "auracle/format.hh"
#include "auracle/sort.hh"
#include "auracle/terminal.hh"

namespace {

constexpr std::string_view kAurBaseurl = "https://aur.archlinux.org";
constexpr std::string_view kPacmanConf = "/etc/pacman.conf";

struct Flags {
  bool ParseFromArgv(int* argc, char*** argv);

  std::string baseurl = std::string(kAurBaseurl);
  std::string pacman_config = std::string(kPacmanConf);
  terminal::WantColor color = terminal::WantColor::AUTO;

  auracle::Auracle::CommandOptions command_options;
};

[[noreturn]] void usage() {
  fputs(
      "auracle [options] command [args...]\n"
      "\n"
      "Query the AUR or clone packages.\n"
      "\n"
      "  -h, --help               Show this help\n"
      "      --version            Show software version\n"
      "\n"
      "  -q, --quiet              Output less, when possible\n"
      "  -r, --recurse            Recurse dependencies when cloning\n"
      "      --literal            Disallow regex in searches\n"
      "      --searchby=BY        Change search-by dimension\n"
      "      --color=WHEN         One of 'auto', 'never', or 'always'\n"
      "      --sort=KEY           Sort results in ascending order by KEY\n"
      "      --rsort=KEY          Sort results in descending order by KEY\n"
      "      --resolve-deps=DEPS  Include/exclude dependency types in "
      "recursive operations\n"
      "      --show-file=FILE     File to dump with 'show' command\n"
      "  -C DIR, --chdir=DIR      Change directory to DIR before cloning\n"
      "  -F FMT, --format=FMT     Specify custom output for search and info\n"
      "\n"
      "Commands:\n"
      "  buildorder               Show build order\n"
      "  clone                    Clone or update git repos for packages\n"
      "  info                     Show detailed information\n"
      "  outdated                 Check for updates for foreign packages\n"
      "  rawinfo                  Dump unformatted JSON for info query\n"
      "  rawsearch                Dump unformatted JSON for search query\n"
      "  resolve                  Resolve dependency strings\n"
      "  search                   Search for packages\n"
      "  show                     Dump package source file\n"
      "  update                   Clone out of date foreign packages\n",
      stdout);
  exit(0);
}

[[noreturn]] void version() {
  std::cout << "auracle " << PROJECT_VERSION << "\n";
  exit(0);
}

bool Flags::ParseFromArgv(int* argc, char*** argv) {
  enum {
    ARG_COLOR = 1000,
    ARG_LITERAL,
    ARG_SEARCHBY,
    ARG_VERSION,
    ARG_BASEURL,
    ARG_SORT,
    ARG_RSORT,
    ARG_PACMAN_CONFIG,
    ARG_SHOW_FILE,
    ARG_RESOLVE_DEPS,
  };

  static constexpr struct option opts[] = {
      // clang-format off
      { "help",            no_argument,       nullptr, 'h' },
      { "quiet",           no_argument,       nullptr, 'q' },
      { "recurse",         no_argument,       nullptr, 'r' },
      { "chdir",           required_argument, nullptr, 'C' },
      { "color",           required_argument, nullptr, ARG_COLOR },
      { "literal",         no_argument,       nullptr, ARG_LITERAL },
      { "resolve-deps",    required_argument, nullptr, ARG_RESOLVE_DEPS },
      { "rsort",           required_argument, nullptr, ARG_RSORT },
      { "searchby",        required_argument, nullptr, ARG_SEARCHBY },
      { "show-file",       required_argument, nullptr, ARG_SHOW_FILE },
      { "sort",            required_argument, nullptr, ARG_SORT },
      { "version",         no_argument,       nullptr, ARG_VERSION },
      { "format",          required_argument, nullptr, 'F' },

      // These are "private", and intentionally not documented in the manual or
      // usage.
      { "baseurl",         required_argument, nullptr, ARG_BASEURL },
      { "pacmanconfig",    required_argument, nullptr, ARG_PACMAN_CONFIG },
      {},
      // clang-format on
  };

  int opt;
  while ((opt = getopt_long(*argc, *argv, "C:F:hqr", opts, nullptr)) != -1) {
    std::string_view sv_optarg(optarg ? optarg : "");

    switch (opt) {
      case 'h':
        usage();
      case 'q':
        command_options.quiet = true;
        break;
      case 'r':
        command_options.recurse = true;
        break;
      case 'C':
        if (sv_optarg.empty()) {
          std::cerr << "error: meaningless option: -C ''\n";
          return false;
        }
        command_options.directory = optarg;
        break;
      case 'F': {
        auto status = format::Validate(sv_optarg);
        if (!status.ok()) {
          std::cerr << "error: invalid arg to --format (" << status.message()
                    << "): " << sv_optarg << "\n";
          return false;
        }
        command_options.format = optarg;
        break;
      }
      case ARG_LITERAL:
        command_options.allow_regex = false;
        break;
      case ARG_SEARCHBY:
        using SearchBy = aur::SearchRequest::SearchBy;

        command_options.search_by =
            aur::SearchRequest::ParseSearchBy(sv_optarg);
        if (command_options.search_by == SearchBy::INVALID) {
          std::cerr << "error: invalid arg to --searchby: " << sv_optarg
                    << "\n";
          return false;
        }
        break;
      case ARG_COLOR:
        if (sv_optarg == "auto") {
          color = terminal::WantColor::AUTO;
        } else if (sv_optarg == "never") {
          color = terminal::WantColor::NO;
        } else if (sv_optarg == "always") {
          color = terminal::WantColor::YES;
        } else {
          std::cerr << "error: invalid arg to --color: " << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_SORT:
        command_options.sorter =
            sort::MakePackageSorter(sv_optarg, sort::OrderBy::ORDER_ASC);
        if (command_options.sorter == nullptr) {
          std::cerr << "error: invalid arg to --sort: " << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_RSORT:
        command_options.sorter =
            sort::MakePackageSorter(sv_optarg, sort::OrderBy::ORDER_DESC);
        if (command_options.sorter == nullptr) {
          std::cerr << "error: invalid arg to --rsort: " << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_RESOLVE_DEPS:
        if (!ParseDependencyKinds(sv_optarg,
                                  &command_options.resolve_depends)) {
          std::cerr << "error: invalid argument to --resolve-deps: "
                    << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_BASEURL:
        baseurl = optarg;
        break;
      case ARG_PACMAN_CONFIG:
        pacman_config = optarg;
        break;
      case ARG_VERSION:
        version();
        break;
      case ARG_SHOW_FILE:
        command_options.show_file = optarg;
        break;
      default:
        return false;
    }
  }

  *argc -= optind - 1;
  *argv += optind - 1;

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Flags flags;
  if (!flags.ParseFromArgv(&argc, &argv)) {
    return 1;
  }

  if (argc < 2) {
    std::cerr << "error: no operation specified (use -h for help)\n";
    return 1;
  }

  std::setlocale(LC_ALL, "");
  terminal::Init(flags.color);

  const auto pacman = auracle::Pacman::NewFromConfig(flags.pacman_config);
  if (pacman == nullptr) {
    std::cerr << "error: failed to parse " << flags.pacman_config << "\n";
    return 1;
  }

  auracle::Auracle auracle(auracle::Auracle::Options()
                               .set_aur_baseurl(flags.baseurl)
                               .set_pacman(pacman.get()));

  const std::string_view action(argv[1]);
  const std::vector<std::string> args(argv + 2, argv + argc);

  const absl::flat_hash_map<std::string_view,
                            int (auracle::Auracle::*)(
                                const std::vector<std::string>&,
                                const auracle::Auracle::CommandOptions&)>
      cmds{
          // clang-format off
          {"buildorder",  &auracle::Auracle::BuildOrder},
          {"clone",       &auracle::Auracle::Clone},
          {"download",    &auracle::Auracle::Clone},
          {"info",        &auracle::Auracle::Info},
          {"rawinfo",     &auracle::Auracle::RawInfo},
          {"rawsearch",   &auracle::Auracle::RawSearch},
          {"outdated",    &auracle::Auracle::Outdated},
          {"resolve",     &auracle::Auracle::Resolve},
          {"search",      &auracle::Auracle::Search},
          {"show",        &auracle::Auracle::Show},
          {"sync",        &auracle::Auracle::Outdated},
          {"update",      &auracle::Auracle::Update},
          // clang-format on
      };

  const auto iter = cmds.find(action);
  if (iter == cmds.end()) {
    std::cerr << "Unknown action " << action << "\n";
    return 1;
  }

  return (auracle.*iter->second)(args, flags.command_options) < 0 ? 1 : 0;
}

/* vim: set et ts=2 sw=2: */
