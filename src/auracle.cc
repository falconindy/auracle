#include "auracle.hh"

#include <errno.h>
#include <getopt.h>
#include <locale.h>

#include <archive.h>
#include <archive_entry.h>

#include <charconv>
#include <filesystem>
#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <string_view>

#include "aur/response.hh"
#include "format.hh"
#include "pacman.hh"
#include "sort.hh"
#include "terminal.hh"

constexpr char kAurBaseurl[] = "https://aur.archlinux.org";
constexpr char kPacmanConf[] = "/etc/pacman.conf";

namespace fs = std::filesystem;

using SearchBy = aur::SearchRequest::SearchBy;

namespace {

int ErrorNotEnoughArgs() {
  std::cerr << "error: not enough arguments.\n";
  return -EINVAL;
}

void FormatLong(std::ostream& os, const std::vector<aur::Package>& packages,
                const dlr::Pacman* pacman) {
  for (const auto& p : packages) {
    auto local_pkg = pacman->GetLocalPackage(p.name);
    os << format::Long(p, std::get_if<dlr::Pacman::Package>(&local_pkg))
       << "\n";
  }
}

void FormatNameOnly(std::ostream& os,
                    const std::vector<aur::Package>& packages) {
  for (const auto& p : packages) {
    os << format::NameOnly(p);
  }
}

void FormatShort(std::ostream& os, const std::vector<aur::Package>& packages) {
  for (const auto& p : packages) {
    os << format::Short(p);
  }
}

void SortUnique(std::vector<aur::Package>* packages,
                const sort::Sorter& sorter) {
  std::sort(packages->begin(), packages->end(), sorter);

  auto iter = std::unique(packages->begin(), packages->end());
  packages->resize(iter - packages->begin());
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
    return -archive_errno(archive);
  }

  while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
    r = archive_read_extract(archive, entry, archive_flags);
    if (r == ARCHIVE_FATAL || r == ARCHIVE_WARN) {
      r = -archive_errno(archive);
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

int Auracle::Info(const std::vector<PackageOrDependency>& args,
                  const CommandOptions& options) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  std::vector<aur::Package> packages;
  aur_.QueueRpcRequest(
      aur::InfoRequest(std::vector<std::string>(args.begin(), args.end())),
      [&](aur::StatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
        } else {
          auto& results = response.value().results;
          std::copy(std::make_move_iterator(results.begin()),
                    std::make_move_iterator(results.end()),
                    std::back_inserter(packages));
        }
        return 0;
      });

  auto r = aur_.Wait();
  if (r) {
    return r;
  }

  if (packages.size() == 0) {
    return -ENOENT;
  }

  // It's unlikely, but still possible that the results may not be unique when
  // our query is large enough that it needs to be split into multiple requests.
  SortUnique(&packages, options.sorter);

  FormatLong(std::cout, packages, pacman_);

  return 0;
}

int Auracle::Search(const std::vector<PackageOrDependency>& args,
                    const CommandOptions& options) {
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
      return -EINVAL;
    }
  }

  const auto matches = [&patterns, &options](const aur::Package& p) -> bool {
    return std::all_of(patterns.begin(), patterns.end(),
                       [&p, &options](const std::regex& re) {
                         switch (options.search_by) {
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

  std::vector<aur::Package> packages;
  for (size_t i = 0; i < args.size(); ++i) {
    aur::SearchRequest r;
    r.SetSearchBy(options.search_by);

    if (options.allow_regex) {
      auto frag = SearchFragFromRegex(args[i]);
      if (frag.empty()) {
        std::cerr << "error: search string '" << std::string(args[i])
                  << "' insufficient for searching by regular expression.\n";
        return -EINVAL;
      }
      r.AddArg(frag);
    } else {
      r.AddArg(args[i]);
    }

    aur_.QueueRpcRequest(
        r, [&, arg{args[i]}](aur::StatusOr<aur::RpcResponse> response) {
          if (!response.ok()) {
            std::cerr << "error: request failed for '" << std::string(arg)
                      << "': " << response.error() << "\n";
            return -EIO;
          } else {
            const auto& results = response.value().results;
            std::copy_if(std::make_move_iterator(results.begin()),
                         std::make_move_iterator(results.end()),
                         std::back_inserter(packages), matches);
          }
          return 0;
        });
  }

  int r = aur_.Wait();
  if (r < 0) {
    return r;
  }

  SortUnique(&packages, options.sorter);

  if (options.quiet) {
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
      info_request, [this, state, want{std::move(args)}](
                        aur::StatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
          return -EIO;
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

          if (state->callback) {
            state->callback(*p);
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

bool ChdirIfNeeded(const fs::path& target) {
  if (target.empty()) {
    return true;
  }

  std::error_code ec;
  fs::current_path(target, ec);
  if (ec.value() != 0) {
    std::cerr << "error: failed to change directory to " << target << ": "
              << ec.message() << "\n";
    return false;
  }

  return true;
}

int Auracle::Clone(const std::vector<PackageOrDependency>& args,
                   const CommandOptions& options) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  if (!ChdirIfNeeded(options.directory)) {
    return -EINVAL;
  }

  int ret = 0;
  PackageIterator iter(options.recurse, [this, &ret](const aur::Package& p) {
    aur_.QueueCloneRequest(
        aur::CloneRequest(p.pkgbase),
        [&ret, pkgbase{p.pkgbase}](aur::StatusOr<aur::CloneResponse> response) {
          if (response.ok()) {
            std::cout << response.value().operation << " complete: "
                      << (fs::current_path() / pkgbase).string() << "\n";
          } else {
            ret = -EIO;
          }
          return 0;
        });
  });

  IteratePackages(args, &iter);

  int r = aur_.Wait();
  if (r < 0) {
    return r;
  }

  if (iter.package_repo.size() == 0) {
    return -ENOENT;
  }

  return ret;
}

int Auracle::Download(const std::vector<PackageOrDependency>& args,
                      const CommandOptions& options) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  if (!ChdirIfNeeded(options.directory)) {
    return -EINVAL;
  }

  PackageIterator iter(options.recurse, [this](const aur::Package& p) {
    aur_.QueueTarballRequest(
        aur::RawRequest(aur::RawRequest::UrlForTarball(p)),
        [pkgbase{p.pkgbase}](aur::StatusOr<aur::RawResponse> response) {
          if (!response.ok()) {
            std::cerr << "error: request failed: " << response.error() << "\n";
            return -EIO;
          }

          int r = ExtractArchive(response.value().bytes);
          if (r < 0) {
            std::cerr << "error: failed to extract tarball for " << pkgbase
                      << ": " << strerror(-r) << "\n";
            return r;
          }

          std::cout << "download complete: "
                    << (fs::current_path() / pkgbase).string() << "\n";
          return 0;
        });
  });

  IteratePackages(args, &iter);

  int r = aur_.Wait();
  if (r < 0) {
    return r;
  }

  if (iter.package_repo.size() == 0) {
    return -ENOENT;
  }

  return 0;
}

int Auracle::Pkgbuild(const std::vector<PackageOrDependency>& args,
                      const CommandOptions&) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  int resultcount = 0;
  aur_.QueueRpcRequest(
      aur::InfoRequest(std::vector<std::string>(args.begin(), args.end())),
      [this, &resultcount](aur::StatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "request failed: " << response.error() << "\n";
          return 0;
        }

        resultcount = response.value().resultcount;

        const bool print_header = response.value().results.size() > 1;

        for (const auto& arg : response.value().results) {
          aur_.QueuePkgbuildRequest(
              aur::RawRequest(aur::RawRequest::UrlForPkgbuild(arg)),
              [print_header, pkgbase{arg.pkgbase}](
                  const aur::StatusOr<aur::RawResponse> response) {
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

  int r = aur_.Wait();
  if (r < 0) {
    return r;
  }

  if (resultcount == 0) {
    return -ENOENT;
  }

  return 0;
}

int Auracle::BuildOrder(const std::vector<PackageOrDependency>& args,
                        const CommandOptions&) {
  if (args.size() == 0) {
    return ErrorNotEnoughArgs();
  }

  PackageIterator iter(/* recurse = */ true, nullptr);
  IteratePackages(args, &iter);

  int r = aur_.Wait();
  if (r < 0) {
    return r;
  }

  if (iter.package_repo.size() == 0) {
    return -ENOENT;
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

int Auracle::Sync(const std::vector<PackageOrDependency>& args,
                  const CommandOptions& options) {
  aur::InfoRequest info_request;

  const auto foreign_pkgs = pacman_->ForeignPackages();
  for (const auto& pkg : foreign_pkgs) {
    if (args.empty() ||
        std::find(args.cbegin(), args.cend(), pkg.pkgname) != args.cend()) {
      info_request.AddArg(pkg.pkgname);
    }
  }

  aur_.QueueRpcRequest(
      info_request, [&](aur::StatusOr<aur::RpcResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
          return -EIO;
        }

        for (const auto& r : response.value().results) {
          auto iter = std::find_if(foreign_pkgs.cbegin(), foreign_pkgs.cend(),
                                   [&r](const dlr::Pacman::Package& p) {
                                     return p.pkgname == r.name;
                                   });
          if (dlr::Pacman::Vercmp(r.version, iter->pkgver) > 0) {
            if (options.quiet) {
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

int Auracle::RawSearch(const std::vector<PackageOrDependency>& args,
                       const CommandOptions& options) {
  for (const auto& arg : args) {
    aur::SearchRequest request;
    request.AddArg(arg);
    request.SetSearchBy(options.search_by);

    aur_.QueueRawRpcRequest(
        request, [](aur::StatusOr<aur::RawResponse> response) {
          if (!response.ok()) {
            std::cerr << "error: request failed: " << response.error() << "\n";
            return -EIO;
          }

          std::cout << response.value().bytes << "\n";
          return 0;
        });
  }

  return aur_.Wait();
}

int Auracle::RawInfo(const std::vector<PackageOrDependency>& args,
                     const CommandOptions&) {
  aur::InfoRequest request;

  for (const auto& arg : args) {
    request.AddArg(arg);
  }

  aur_.QueueRawRpcRequest(
      request, [](aur::StatusOr<aur::RawResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
          return -EIO;
        }

        std::cout << response.value().bytes << "\n";
        return 0;
      });

  return aur_.Wait();
}

struct Flags {
  std::string baseurl = kAurBaseurl;
  int max_connections = 20;
  int connect_timeout = 10;
  terminal::WantColor color = terminal::WantColor::AUTO;

  Auracle::CommandOptions command_options;
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
      "  clone                    Clone or update git repos for packages\n"
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
    BASEURL,
    SORT,
    RSORT,
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
      { "rsort",             required_argument, 0, RSORT },
      { "searchby",          required_argument, 0, SEARCHBY },
      { "sort",              required_argument, 0, SORT },
      { "version",           no_argument,       0, VERSION },
      { "baseurl",           required_argument, 0, BASEURL },
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
        flags->command_options.quiet = true;
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
      case LITERAL:
        flags->command_options.allow_regex = false;
        break;
      case SEARCHBY:
        flags->command_options.search_by =
            aur::SearchRequest::ParseSearchBy(sv_optarg);
        if (flags->command_options.search_by == SearchBy::INVALID) {
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
      case SORT:
        flags->command_options.sorter =
            sort::MakePackageSorter(sv_optarg, sort::OrderBy::ORDER_ASC);
        if (flags->command_options.sorter == nullptr) {
          std::cerr << "error: invalid arg to --sort: " << sv_optarg << "\n";
        }
        break;
      case RSORT:
        flags->command_options.sorter =
            sort::MakePackageSorter(sv_optarg, sort::OrderBy::ORDER_DESC);
        if (flags->command_options.sorter == nullptr) {
          std::cerr << "error: invalid arg to --rsort: " << sv_optarg << "\n";
        }
        break;
      case BASEURL:
        flags->baseurl = std::string(sv_optarg);
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
                      .set_aur_baseurl(flags.baseurl)
                      .set_pacman(pacman.get())
                      .set_connection_timeout(flags.connect_timeout)
                      .set_max_connections(flags.max_connections));

  std::string_view action(argv[1]);
  std::vector<PackageOrDependency> args(argv + 2, argv + argc);

  std::unordered_map<std::string_view,
                     int (Auracle::*)(
                         const std::vector<PackageOrDependency>& args,
                         const Auracle::CommandOptions& options)>
      cmds{
          {"buildorder", &Auracle::BuildOrder},
          {"clone", &Auracle::Clone},
          {"download", &Auracle::Download},
          {"info", &Auracle::Info},
          {"pkgbuild", &Auracle::Pkgbuild},
          {"rawinfo", &Auracle::RawInfo},
          {"rawsearch", &Auracle::RawSearch},
          {"search", &Auracle::Search},
          {"sync", &Auracle::Sync},
      };

  if (auto iter = cmds.find(action); iter != cmds.end()) {
    return (auracle.*iter->second)(args, flags.command_options) < 0 ? 1 : 0;
  } else {
    std::cerr << "Unknown action " << action << "\n";
    return 1;
  }
}

/* vim: set et ts=2 sw=2: */
