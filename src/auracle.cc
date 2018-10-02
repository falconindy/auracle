#include "auracle.hh"

#include <errno.h>
#include <getopt.h>
#include <locale.h>

#include <charconv>
#include <filesystem>
#include <functional>
#include <memory>
#include <regex>
#include <string_view>

#include "aur/response.hh"
#include "format.hh"
#include "pacman.hh"
#include "sort.hh"
#include "terminal.hh"

#include <set>
#include <string>

#include <archive.h>
#include <archive_entry.h>

constexpr char kAurBaseurl[] = "https://aur.archlinux.org";
constexpr char kPacmanConf[] = "/etc/pacman.conf";

namespace fs = std::filesystem;

using SearchBy = aur::SearchRequest::SearchBy;

namespace {

int ErrorNotEnoughArgs() {
  std::cerr << "error: not enough arguments.\n";
  return 1;
}

template <typename Container>
void FormatLong(std::ostream& os, const Container& packages,
                const dlr::Pacman* pacman) {
  for (const auto& p : packages) {
    auto local_pkg = pacman->GetLocalPackage(p.name);
    os << format::Long(p, std::get_if<dlr::Pacman::Package>(&local_pkg))
       << "\n";
  }
}

template <typename Container>
void FormatNameOnly(std::ostream& os, const Container& packages) {
  for (const auto& p : packages) {
    os << format::NameOnly(p);
  }
}

template <typename Container>
void FormatShort(std::ostream& os, const Container& packages) {
  for (const auto& p : packages) {
    os << format::Short(p);
  }
}

int ExtractArchive(const std::string& archive_bytes) {
  struct archive* archive;
  struct archive_entry* entry;
  const int archive_flags = ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME;
  int r = 0;

  archive = archive_read_new();
  archive_read_support_filter_all(archive);
  archive_read_support_format_all(archive);

  if (archive_read_open_memory(archive, archive_bytes.data(),
                               archive_bytes.size()) != ARCHIVE_OK) {
    return 1;
  }

  while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
    r = archive_read_extract(archive, entry, archive_flags);
    if (r == ARCHIVE_FATAL || r == ARCHIVE_WARN) {
      r = archive_errno(archive);
      break;
    } else if (r == ARCHIVE_EOF) {
      r = 0;
      break;
    }
  }

  archive_read_close(archive);
  archive_read_free(archive);

  return r;
}

std::vector<std::string> NotFoundPackages(
    const std::vector<PackageOrDependency>& want,
    const std::vector<aur::Package>& got, const dlr::InMemoryRepo& repo) {
  std::vector<std::string> missing;

  for (const auto& p : want) {
    if (repo.LookupByPkgname(p) != nullptr) {
      continue;
    }

    if (std::find_if(got.cbegin(), got.cend(), [&p](const aur::Package& pkg) {
          return pkg.name == std::string(p);
        }) != got.cend()) {
      continue;
    }

    missing.push_back(p);
  }

  return missing;
}

}  // namespace

std::string SearchFragFromRegex(const std::string& s) {
  static constexpr char kRegexChars[] = "^.+*?$[](){}|\\";

  const char* arg = s.data();

  int span = 0;
  const char* argstr;
  for (argstr = arg; *argstr; argstr++) {
    span = strcspn(argstr, kRegexChars);

    /* given 'cow?', we can't include w in the search */
    if (argstr[span] == '?' || argstr[span] == '*') {
      span--;
    }

    /* a string inside [] or {} cannot be a valid span */
    if (strchr("[{", *argstr)) {
      argstr = strpbrk(argstr + span, "]}");
      if (!argstr) {
        return "";
      }
      continue;
    }

    if (span >= 2) {
      break;
    }
  }

  if (span < 2) {
    return "";
  }

  return std::string(argstr, span);
}

int Auracle::Info(const std::vector<PackageOrDependency>& args) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  aur::InfoRequest request;

  for (const auto& arg : args) {
    request.AddArg(arg);
  }

  int resultcount = 0;
  aur_.QueueRpcRequest(
      &request,
      [this, &resultcount](aur::HttpStatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
        } else {
          const auto& result = response.value();
          FormatLong(std::cout, result.results, pacman_);
          resultcount = result.resultcount;
        }
        return 0;
      });

  auto r = aur_.Wait();
  if (r) {
    return r;
  }

  if (resultcount == 0) {
    return 1;
  }

  return 0;
}

int Auracle::Search(const std::vector<PackageOrDependency>& args, SearchBy by) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  std::vector<std::regex> patterns;
  for (const auto& arg : args) {
    try {
      patterns.emplace_back(std::string(arg),
                            std::regex::icase | std::regex::optimize);
    } catch (const std::regex_error& e) {
      std::cerr << "error: invalid regex: " << std::string(arg) << "\n";
      return 1;
    }
  }

  std::vector<aur::SearchRequest> requests(args.size());
  for (size_t i = 0; i < args.size(); ++i) {
    auto r = &requests[i];

    r->SetSearchBy(by);

    if (allow_regex_) {
      auto frag = SearchFragFromRegex(args[i]);
      if (frag.empty()) {
        std::cerr << "error: search string '" << std::string(args[i])
                  << "' insufficient for searching by regular expression.\n";
        return 1;
      }
      r->AddArg(frag);
    } else {
      r->AddArg(args[i]);
    }
  }

  const auto matches = [&patterns, by](const aur::Package& p) -> bool {
    return std::all_of(patterns.begin(), patterns.end(),
                       [&p, by](const std::regex& re) {
                         switch (by) {
                           case SearchBy::NAME:
                             return std::regex_search(p.name, re);
                           case SearchBy::NAME_DESC:
                             return std::regex_search(p.name, re) ||
                                    std::regex_search(p.description, re);
                           default:
                             // The AUR only matches maintainer and *depends
                             // fields exactly so there's no point in doing
                             // additional filtering on these types.
                             return true;
                         }
                       });
  };

  std::set<aur::Package, sort::PackageNameLess> packages;
  for (size_t i = 0; i < args.size(); ++i) {
    aur_.QueueRpcRequest(
        &requests[i],
        [&, arg{args[i]}](aur::HttpStatusOr<aur::RpcResponse> response) {
          if (!response.ok()) {
            std::cerr << "error: request failed for '" << std::string(arg)
                      << "': " << response.error() << "\n";
            return 1;
          } else {
            const auto& results = response.value().results;
            std::copy_if(std::make_move_iterator(results.begin()),
                         std::make_move_iterator(results.end()),
                         std::inserter(packages, packages.end()), matches);
          }
          return 0;
        });
  }

  if (aur_.Wait() != 0) {
    return 1;
  }

  // TODO: implement custom sorting
  if (options_.quiet) {
    FormatNameOnly(std::cout, packages);
  } else {
    FormatShort(std::cout, packages);
  }

  return 0;
}

void Auracle::IteratePackages(std::vector<PackageOrDependency> args,
                              Auracle::PackageIterator* state) {
  aur::InfoRequest info_request;

  for (const auto& arg : args) {
    if (pacman_->ShouldIgnorePackage(arg)) {
      continue;
    }

    if (state->package_repo.LookupByPkgname(arg) != nullptr) {
      continue;
    }

    if (const auto repo = pacman_->RepoForPackage(arg); !repo.empty()) {
      std::cout << std::string(arg) << " is available in " << repo << "\n";
      continue;
    }

    info_request.AddArg(arg);
  }

  aur_.QueueRpcRequest(
      &info_request, [this, state, want{std::move(args)}](
                         aur::HttpStatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
          return 1;
        }

        const auto& results = response.value().results;

        for (const auto& p :
             NotFoundPackages(want, results, state->package_repo)) {
          std::cerr << "no results found for " << p << "\n";
        }

        for (auto& result : results) {
          // check for the pkgbase existing in our repo
          const bool have_pkgbase =
              state->package_repo.LookupByPkgbase(result.pkgbase) != nullptr;

          // Regardless, try to add the package, as it might be another member
          // of the same pkgbase.
          auto [p, added] = state->package_repo.AddPackage(std::move(result));

          if (!added || have_pkgbase) {
            continue;
          }

          if (state->download) {
            aur::RawRequest request(aur::RawRequest::UrlForTarball(*p));
            aur_.QueueTarballRequest(
                &request, [pkgbase{p->pkgbase}](
                              aur::HttpStatusOr<aur::RawResponse> response) {
                  if (!response.ok()) {
                    std::cerr << "error: request failed: " << response.error()
                              << "\n";
                    return 1;
                  }

                  int r = ExtractArchive(response.value().bytes);
                  if (r != 0) {
                    std::cerr << "error: failed to extract tarball for "
                              << pkgbase << ": " << strerror(r) << "\n";
                    return r;
                  }

                  std::cout << "download complete: "
                            << (fs::current_path() / pkgbase).string() << "\n";
                  return 0;
                });
          }

          if (state->recurse) {
            std::vector<PackageOrDependency> alldeps;
            alldeps.reserve(p->depends.size() + p->makedepends.size() +
                            p->checkdepends.size());

            for (const auto* deparray :
                 {&p->depends, &p->makedepends, &p->checkdepends}) {
              for (const auto& dep : *deparray) {
                if (!pacman_->RepoForPackage(dep.depstring).empty()) {
                  continue;
                }

                alldeps.push_back(dep);
              }
            }

            IteratePackages(std::move(alldeps), state);
          }
        }

        return 0;
      });
}

int Auracle::Download(const std::vector<PackageOrDependency>& args,
                      bool recurse, const fs::path& directory) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  if (!directory.empty()) {
    std::error_code ec;
    fs::current_path(directory, ec);
    if (ec.value() != 0) {
      std::cerr << "error: failed to change directory to " << directory << ": "
                << ec.message() << "\n";
      return 1;
    }
  }

  PackageIterator iter(recurse, /* download = */ true);
  IteratePackages(args, &iter);

  if (aur_.Wait() != 0 || iter.package_repo.size() == 0) {
    return 1;
  }

  return 0;
}

int Auracle::Pkgbuild(const std::vector<PackageOrDependency>& args) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  aur::InfoRequest info_request;
  for (const auto& arg : args) {
    info_request.AddArg(arg);
  }

  int resultcount = 0;
  aur_.QueueRpcRequest(
      &info_request,
      [this, &resultcount](aur::HttpStatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "request failed: " << response.error() << "\n";
          return 0;
        }

        resultcount = response.value().resultcount;

        const bool print_header = response.value().results.size() > 1;

        std::vector<aur::RawRequest> requests;
        for (const auto& arg : response.value().results) {
          const auto& r =
              requests.emplace_back(aur::RawRequest::UrlForPkgbuild(arg));
          aur_.QueuePkgbuildRequest(
              &r, [print_header, pkgbase{arg.pkgbase}](
                      const aur::HttpStatusOr<aur::RawResponse> response) {
                if (!response.ok()) {
                  std::cerr << "request failed: " << response.error() << "\n";
                  return 1;
                }

                if (print_header) {
                  std::cout << "### BEGIN " << pkgbase << "/PKGBUILD\n";
                }
                std::cout << response.value().bytes << "\n";
                return 0;
              });
        }

        return 0;
      });

  aur_.Wait();

  if (resultcount == 0) {
    return 1;
  }

  return 0;
}

int Auracle::BuildOrder(const std::vector<PackageOrDependency>& args) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  PackageIterator iter(/* recurse = */ true, /* download = */ false);
  IteratePackages(args, &iter);

  if (aur_.Wait() != 0 || iter.package_repo.size() == 0) {
    return 1;
  }

  std::vector<const aur::Package*> total_ordering;
  for (const auto& arg : args) {
    const auto name = std::string(arg);

    iter.package_repo.WalkDependencies(
        name, [&total_ordering](const aur::Package* package) {
          total_ordering.push_back(package);
        });
  }

  // TODO: dedupe for when multiple args given.
  for (const auto& p : total_ordering) {
    if (pacman_->DependencyIsSatisfied(p->name)) {
      continue;
    }

    std::cout << "BUILD " << p->name << "\n";
  }

  return 0;
}

int Auracle::Sync(const std::vector<PackageOrDependency>& args) {
  aur::InfoRequest info_request;

  const auto foreign_pkgs = pacman_->ForeignPackages();
  for (const auto& pkg : foreign_pkgs) {
    if (args.empty() ||
        std::find(args.cbegin(), args.cend(), pkg.pkgname) != args.cend()) {
      info_request.AddArg(pkg.pkgname);
    }
  }

  aur_.QueueRpcRequest(
      &info_request, [&](aur::HttpStatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
          return 1;
        }

        for (const auto& r : response.value().results) {
          auto iter = std::find_if(foreign_pkgs.cbegin(), foreign_pkgs.cend(),
                                   [&r](const dlr::Pacman::Package& p) {
                                     return p.pkgname == r.name;
                                   });
          if (dlr::Pacman::Vercmp(r.version, iter->pkgver) > 0) {
            if (options_.quiet) {
              std::cout << format::NameOnly(r);
            } else {
              std::cout << format::Update(*iter, r,
                                          pacman_->ShouldIgnorePackage(r.name));
            }
          }
        }

        return 0;
      });

  return aur_.Wait();
}

int Auracle::SendRawRpc(const aur::RpcRequest* request) {
  aur_.QueueRawRpcRequest(
      request, [](aur::HttpStatusOr<aur::RawResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
          return 1;
        }

        std::cout << response.value().bytes;
        return 0;
      });

  return aur_.Wait();
}

int Auracle::RawSearch(const std::vector<PackageOrDependency>& args,
                       aur::SearchRequest::SearchBy by) {
  aur::SearchRequest search_request;

  search_request.SetSearchBy(by);
  for (const auto& arg : args) {
    search_request.AddArg(arg);
  }

  return SendRawRpc(&search_request);
}

int Auracle::RawInfo(const std::vector<PackageOrDependency>& args) {
  aur::InfoRequest info_request;

  for (const auto& arg : args) {
    info_request.AddArg(arg);
  }

  return SendRawRpc(&info_request);
}

struct Flags {
  SearchBy search_by = SearchBy::NAME_DESC;
  int max_connections = 20;
  int connect_timeout = 10;
  bool recurse = false;
  bool allow_regex = true;
  bool quiet = false;
  terminal::WantColor color = terminal::WantColor::AUTO;
  fs::path directory;
};

__attribute__((noreturn)) void usage(void) {
  fputs(
      "auracle [options] command\n"
      "\n"
      "Query the AUR or download snapshots of packages.\n"
      "\n"
      "  -h, --help               Show this help\n"
      "      --version            Show software version\n"
      "\n"
      "  -q, --quiet              Output less, when possible\n"
      "  -r, --recurse            Recurse through dependencies on download\n"
      "      --literal            Disallow regex in searches\n"
      "      --searchby=BY        Change search-by dimension\n"
      "      --connect-timeout=N  Set connection timeout in seconds\n"
      "      --max-connections=N  Limit active connections\n"
      "      --color=WHEN         One of 'auto', 'never', or 'always'\n"
      "  -C DIR, --chdir=DIR      Change directory to DIR before downloading\n"
      "\n"
      "Commands:\n"
      "  buildorder               Show build order\n"
      "  download                 Download tarball snapshots\n"
      "  info                     Show detailed information\n"
      "  pkgbuild                 Show PKGBUILDs\n"
      "  search                   Search for packages\n"
      "  sync                     Check for updates for foreign packages\n"
      "  rawinfo                  Dump unformatted JSON for info query\n"
      "  rawsearch                Dump unformatted JSON for search query\n",
      stdout);
  exit(0);
}

__attribute__((noreturn)) void version(void) {
  std::cout << "auracle " << PACKAGE_VERSION << "\n";
  exit(0);
}

bool ParseFlags(int* argc, char*** argv, Flags* flags) {
  enum {
    COLOR = 1000,
    CONNECT_TIMEOUT,
    LITERAL,
    MAX_CONNECTIONS,
    SEARCHBY,
    VERSION,
  };

  static constexpr struct option opts[] = {
      // clang-format off
      { "help",              no_argument,       0, 'h' },
      { "quiet",             no_argument,       0, 'q' },
      { "recurse",           no_argument,       0, 'r' },

      { "chdir",             required_argument, 0, 'C' },
      { "color",             required_argument, 0, COLOR },
      { "connect-timeout",   required_argument, 0, CONNECT_TIMEOUT },
      { "literal",           no_argument,       0, LITERAL },
      { "max-connections",   required_argument, 0, MAX_CONNECTIONS },
      { "searchby",          required_argument, 0, SEARCHBY },
      { "version",           no_argument,       0, VERSION },
      // clang-format on
  };

  const auto stoi = [](std::string_view s, int* i) -> bool {
    return std::from_chars(s.data(), s.data() + s.size(), *i).ec == std::errc{};
  };

  int opt;
  while ((opt = getopt_long(*argc, *argv, "C:hqr", opts, nullptr)) != -1) {
    std::string_view sv_optarg(optarg);

    switch (opt) {
      case 'h':
        usage();
      case 'q':
        flags->quiet = true;
        break;
      case 'r':
        flags->recurse = true;
        break;
      case 'C':
        if (sv_optarg.empty()) {
          std::cerr << "error: meaningless option: -C ''\n";
          return false;
        }
        flags->directory = optarg;
        break;
      case LITERAL:
        flags->allow_regex = false;
        break;
      case SEARCHBY:
        flags->search_by = aur::SearchRequest::ParseSearchBy(sv_optarg);
        if (flags->search_by == SearchBy::INVALID) {
          std::cerr << "error: invalid arg to --searchby: " << sv_optarg
                    << "\n";
          return false;
        }
        break;
      case CONNECT_TIMEOUT:
        if (!stoi(sv_optarg, &flags->connect_timeout) ||
            flags->max_connections < 0) {
          std::cerr << "error: invalid value to --connect-timeout: "
                    << sv_optarg << "\n";
          return false;
        }
        break;
      case MAX_CONNECTIONS:
        if (!stoi(sv_optarg, &flags->max_connections) ||
            flags->max_connections < 0) {
          std::cerr << "error: invalid value to --max-connections: "
                    << sv_optarg << "\n";
          return false;
        }
        break;
      case COLOR:
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
      case VERSION:
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

int main(int argc, char** argv) {
  Flags flags;
  if (!ParseFlags(&argc, &argv, &flags)) {
    return 1;
  }

  if (argc < 2) {
    std::cerr << "error: no operation specified (use -h for help)\n";
    return 1;
  }

  setlocale(LC_ALL, "");
  terminal::Init(flags.color);

  const auto pacman = dlr::Pacman::NewFromConfig(kPacmanConf);
  if (pacman == nullptr) {
    std::cerr << "error: failed to parse " << kPacmanConf << "\n";
    return 1;
  }

  Auracle auracle(Auracle::Options()
                      .set_aur_baseurl(kAurBaseurl)
                      .set_pacman(pacman.get())
                      .set_quiet(flags.quiet)
                      .set_allow_regex(flags.allow_regex)
                      .set_connection_timeout(flags.connect_timeout)
                      .set_max_connections(flags.max_connections));

  std::string_view action(argv[1]);
  std::vector<PackageOrDependency> args(argv + 2, argv + argc);

  if (action == "search") {
    return auracle.Search(args, flags.search_by);
  } else if (action == "info") {
    return auracle.Info(args);
  } else if (action == "download") {
    return auracle.Download(args, flags.recurse, flags.directory);
  } else if (action == "sync") {
    return auracle.Sync(args);
  } else if (action == "buildorder") {
    return auracle.BuildOrder(args);
  } else if (action == "pkgbuild") {
    return auracle.Pkgbuild(args);
  } else if (action == "rawinfo") {
    return auracle.RawInfo(args);
  } else if (action == "rawsearch") {
    return auracle.RawSearch(args, flags.search_by);
  }

  std::cerr << "Unknown action " << action << "\n";
  return 1;
}

/* vim: set et ts=2 sw=2: */
