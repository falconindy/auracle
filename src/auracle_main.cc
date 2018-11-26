#include <getopt.h>

#include <charconv>
#include <clocale>
#include <iostream>

#include "auracle/auracle.hh"
#include "auracle/sort.hh"
#include "auracle/terminal.hh"

namespace {

constexpr char kAurBaseurl[] = "https://aur.archlinux.org";
constexpr char kPacmanConf[] = "/etc/pacman.conf";

struct Flags {
  std::string baseurl = kAurBaseurl;
  std::string pacman_config = kPacmanConf;
  int max_connections = 5;
  int connect_timeout = 10;
  terminal::WantColor color = terminal::WantColor::AUTO;

  auracle::Auracle::CommandOptions command_options;
};

__attribute__((noreturn)) void usage() {
  fputs(
      "auracle [options] command\n"
      "\n"
      "Query the AUR or download snapshots of packages.\n"
      "\n"
      "  -h, --help               Show this help\n"
      "      --version            Show software version\n"
      "\n"
      "  -q, --quiet              Output less, when possible\n"
      "  -v[N], --verbose[=N]     Increase verbosity of output (value of N sets level;\n"
      "                               without argument increases level by 1)"
      "\n"
      "  -r, --recurse            Recurse through dependencies on download\n"
      "      --literal            Disallow regex in searches\n"
      "      --searchby=BY        Change search-by dimension\n"
      "      --connect-timeout=N  Set connection timeout in seconds\n"
      "      --max-connections=N  Limit active connections\n"
      "      --color=WHEN         One of 'auto', 'never', or 'always'\n"
      "      --sort=KEY           Sort results in ascending order by KEY\n"
      "      --rsort=KEY          Sort results in descending order by KEY\n"
      "  -C DIR, --chdir=DIR      Change directory to DIR before downloading\n"
      "\n"
      "Commands:\n"
      "  buildorder               Show build order\n"
      "  clone                    Clone or update git repos for packages\n"
      "  download                 Download tarball snapshots\n"
      "  info                     Show detailed information\n"
      "  pkgbuild                 Show PKGBUILDs\n"
      "  rawinfo                  Dump unformatted JSON for info query\n"
      "  rawsearch                Dump unformatted JSON for search query\n"
      "  search                   Search for packages\n"
      "  sync                     Check for updates for foreign packages\n",
      stdout);
  exit(0);
}

__attribute__((noreturn)) void version() {
  std::cout << "auracle " << PACKAGE_VERSION << "\n";
  exit(0);
}

bool ParseFlags(int* argc, char*** argv, Flags* flags) {
  enum {
    ARG_COLOR = 1000,
    ARG_CONNECT_TIMEOUT,
    ARG_LITERAL,
    ARG_MAX_CONNECTIONS,
    ARG_SEARCHBY,
    ARG_VERSION,
    ARG_BASEURL,
    ARG_SORT,
    ARG_RSORT,
    ARG_PACMAN_CONFIG,
  };

  static constexpr struct option opts[] = {
      // clang-format off
      { "help",            no_argument,       nullptr, 'h' },
      { "quiet",           no_argument,       nullptr, 'q' },
      { "verbose",         optional_argument, nullptr, 'v' },
      { "recurse",         no_argument,       nullptr, 'r' },
      { "chdir",           required_argument, nullptr, 'C' },
      { "color",           required_argument, nullptr, ARG_COLOR },
      { "connect-timeout", required_argument, nullptr, ARG_CONNECT_TIMEOUT },
      { "literal",         no_argument,       nullptr, ARG_LITERAL },
      { "max-connections", required_argument, nullptr, ARG_MAX_CONNECTIONS },
      { "rsort",           required_argument, nullptr, ARG_RSORT },
      { "searchby",        required_argument, nullptr, ARG_SEARCHBY },
      { "sort",            required_argument, nullptr, ARG_SORT },
      { "version",         no_argument,       nullptr, ARG_VERSION },

      // These are "private", and intentionally not documented in the manual or
      // usage.
      { "baseurl",         required_argument, nullptr, ARG_BASEURL },
      { "pacmanconfig",    required_argument, nullptr, ARG_PACMAN_CONFIG },
      {},
      // clang-format on
  };

  const auto stoi = [](std::string_view s, int* i) -> bool {
    return std::from_chars(s.data(), s.data() + s.size(), *i).ec == std::errc{};
  };

  int opt;
  while ((opt = getopt_long(*argc, *argv, "C:hqv::r", opts, nullptr)) != -1) {
    std::string_view sv_optarg(optarg);

    switch (opt) {
      case 'h':
        usage();
      case 'q':
        flags->command_options.quiet = true;
        break;
      case 'v':
        if (sv_optarg.empty())
          flags->command_options.verbosity += 1;
        else
        {
          int verbosity;
          auto result = std::from_chars(sv_optarg.begin(), sv_optarg.end(), verbosity);

          if (result.ec != std::errc{} || result.ptr != sv_optarg.end())
          {
            std::cerr << "invalid verbosity setting \"" << sv_optarg << "\"\n";
            return false;
          }

          flags->command_options.verbosity = verbosity;
        }
        break;
      case 'r':
        flags->command_options.recurse = true;
        break;
      case 'C':
        if (sv_optarg.empty()) {
          std::cerr << "error: meaningless option: -C ''\n";
          return false;
        }
        flags->command_options.directory = optarg;
        break;
      case ARG_LITERAL:
        flags->command_options.allow_regex = false;
        break;
      case ARG_SEARCHBY:
        using SearchBy = aur::SearchRequest::SearchBy;

        flags->command_options.search_by =
            aur::SearchRequest::ParseSearchBy(sv_optarg);
        if (flags->command_options.search_by == SearchBy::INVALID) {
          std::cerr << "error: invalid arg to --searchby: " << sv_optarg
                    << "\n";
          return false;
        }
        break;
      case ARG_CONNECT_TIMEOUT:
        if (!stoi(sv_optarg, &flags->connect_timeout) ||
            flags->max_connections < 0) {
          std::cerr << "error: invalid value to --connect-timeout: "
                    << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_MAX_CONNECTIONS:
        if (!stoi(sv_optarg, &flags->max_connections) ||
            flags->max_connections < 0) {
          std::cerr << "error: invalid value to --max-connections: "
                    << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_COLOR:
        if (sv_optarg == "auto") {
          flags->color = terminal::WantColor::AUTO;
        } else if (sv_optarg == "never") {
          flags->color = terminal::WantColor::NO;
        } else if (sv_optarg == "always") {
          flags->color = terminal::WantColor::YES;
        } else {
          std::cerr << "error: invalid arg to --color: " << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_SORT:
        flags->command_options.sorter =
            sort::MakePackageSorter(sv_optarg, sort::OrderBy::ORDER_ASC);
        if (flags->command_options.sorter == nullptr) {
          std::cerr << "error: invalid arg to --sort: " << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_RSORT:
        flags->command_options.sorter =
            sort::MakePackageSorter(sv_optarg, sort::OrderBy::ORDER_DESC);
        if (flags->command_options.sorter == nullptr) {
          std::cerr << "error: invalid arg to --rsort: " << sv_optarg << "\n";
          return false;
        }
        break;
      case ARG_BASEURL:
        flags->baseurl = std::string(sv_optarg);
        break;
      case ARG_PACMAN_CONFIG:
        flags->pacman_config = std::string(sv_optarg);
        break;
      case ARG_VERSION:
        version();
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
  if (!ParseFlags(&argc, &argv, &flags)) {
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
                               .set_pacman(pacman.get())
                               .set_connection_timeout(flags.connect_timeout)
                               .set_max_connections(flags.max_connections));

  const std::string_view action(argv[1]);
  const std::vector<std::string> args(argv + 2, argv + argc);

  const std::unordered_map<std::string_view,
                           int (auracle::Auracle::*)(
                               const std::vector<std::string>& args,
                               const auracle::Auracle::CommandOptions& options)>
      cmds{
          {"buildorder", &auracle::Auracle::BuildOrder},
          {"clone", &auracle::Auracle::Clone},
          {"download", &auracle::Auracle::Download},
          {"info", &auracle::Auracle::Info},
          {"pkgbuild", &auracle::Auracle::Pkgbuild},
          {"rawinfo", &auracle::Auracle::RawInfo},
          {"rawsearch", &auracle::Auracle::RawSearch},
          {"search", &auracle::Auracle::Search},
          {"sync", &auracle::Auracle::Sync},
      };

  if (const auto iter = cmds.find(action); iter != cmds.end()) {
    return (auracle.*iter->second)(args, flags.command_options) < 0 ? 1 : 0;
  }

  std::cerr << "Unknown action " << action << "\n";
  return 1;
}

/* vim: set et ts=2 sw=2: */
