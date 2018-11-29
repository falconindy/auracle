#include "auracle.hh"

#include <archive.h>
#include <archive_entry.h>

#include <cerrno>
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

namespace fs = std::filesystem;

namespace auracle {

namespace {

int ErrorNotEnoughArgs() {
  std::cerr << "error: not enough arguments.\n";
  return -EINVAL;
}

void FormatLong(std::ostream& os, const std::vector<aur::Package>& packages,
                const Pacman* pacman) {
  for (const auto& p : packages) {
    auto local_pkg = pacman->GetLocalPackage(p.name);
    os << format::Long(p, local_pkg ? &local_pkg.value() : nullptr) << "\n";
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

  packages->resize(std::unique(packages->begin(), packages->end()) -
                   packages->begin());
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
    }

    if (r == ARCHIVE_EOF) {
      r = 0;
      break;
    }
  }

  archive_read_close(archive);
  archive_read_free(archive);

  return r;
}

std::vector<std::string> NotFoundPackages(
    const std::vector<std::string>& want, const std::vector<aur::Package>& got,
    const auracle::PackageCache& package_cache) {
  std::vector<std::string> missing;

  for (const auto& p : want) {
    if (package_cache.LookupByPkgname(p) != nullptr) {
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

std::string GetSearchFragment(const std::string& s, bool allow_regex) {
  static constexpr char kRegexChars[] = "^.+*?$[](){}|\\";

  if (!allow_regex) {
    return s;
  }

  int span = 0;
  const char* argstr;
  for (argstr = s.data(); *argstr != '\0'; argstr++) {
    span = strcspn(argstr, kRegexChars);

    /* given 'cow?', we can't include w in the search */
    if (argstr[span] == '?' || argstr[span] == '*') {
      span--;
    }

    /* a string inside [] or {} cannot be a valid span */
    if (strchr("[{", *argstr) != nullptr) {
      argstr = strpbrk(argstr + span, "]}");
      if (argstr == nullptr) {
        return std::string();
      }
      continue;
    }

    if (span >= 2) {
      break;
    }
  }

  if (span < 2) {
    return std::string();
  }

  return std::string(argstr, span);
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

const std::string GetRpcError(
    const aur::ResponseWrapper<aur::RpcResponse>& response) {
  if (!response.error().empty()) {
    return response.error();
  }

  if (response.status() != 200) {
    return "unexpected HTTP status code " + std::to_string(response.status());
  }

  return response.value().error;
}

bool RpcResponseIsFailure(
    const aur::ResponseWrapper<aur::RpcResponse>& response) {
  const auto error = GetRpcError(response);
  if (error.empty()) {
    return false;
  }

  std::cerr << "error: " << error << "\n";
  return true;
}

}  // namespace

void Auracle::IteratePackages(std::vector<std::string> args,
                              Auracle::PackageIterator* state) {
  aur::InfoRequest info_request;

  for (const auto& arg : args) {
    if (pacman_->ShouldIgnorePackage(arg)) {
      continue;
    }

    if (state->package_cache.LookupByPkgname(arg) != nullptr) {
      continue;
    }

    if (const auto repo = pacman_->RepoForPackage(arg); !repo.empty()) {
      std::cout << arg << " is available in " << repo << "\n";
      continue;
    }

    info_request.AddArg(arg);
  }

  aur_.QueueRpcRequest(
      info_request, [this, state, want{std::move(args)}](
                        aur::ResponseWrapper<aur::RpcResponse> response) {
        if (RpcResponseIsFailure(response)) {
          return -EIO;
        }

        auto results = std::move(response.value().results);

        for (const auto& p :
             NotFoundPackages(want, results, state->package_cache)) {
          std::cerr << "no results found for " << p << "\n";
        }

        for (auto& result : results) {
          // check for the pkgbase existing in our repo
          const bool have_pkgbase =
              state->package_cache.LookupByPkgbase(result.pkgbase) != nullptr;

          // Regardless, try to add the package, as it might be another member
          // of the same pkgbase.
          auto [p, added] = state->package_cache.AddPackage(std::move(result));

          if (!added || have_pkgbase) {
            continue;
          }

          if (state->callback) {
            state->callback(*p);
          }

          if (state->recurse) {
            std::vector<std::string> alldeps;
            alldeps.reserve(p->depends.size() + p->makedepends.size() +
                            p->checkdepends.size());

            for (const auto* deparray :
                 {&p->depends, &p->makedepends, &p->checkdepends}) {
              for (const auto& dep : *deparray) {
                if (pacman_->HasPackage(dep.depstring)) {
                  continue;
                }

                alldeps.push_back(dep.name);
              }
            }

            IteratePackages(std::move(alldeps), state);
          }
        }

        return 0;
      });
}

int Auracle::Info(const std::vector<std::string>& args,
                  const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  std::vector<aur::Package> packages;
  aur_.QueueRpcRequest(aur::InfoRequest(args),
                       [&](aur::ResponseWrapper<aur::RpcResponse> response) {
                         if (RpcResponseIsFailure(response)) {
                           return -EIO;
                         }

                         auto results = std::move(response.value().results);
                         std::copy(std::make_move_iterator(results.begin()),
                                   std::make_move_iterator(results.end()),
                                   std::back_inserter(packages));

                         return 0;
                       });

  auto r = aur_.Wait();
  if (r < 0) {
    return r;
  }

  if (packages.empty()) {
    return -ENOENT;
  }

  // It's unlikely, but still possible that the results may not be unique when
  // our query is large enough that it needs to be split into multiple requests.
  SortUnique(&packages, options.sorter);

  FormatLong(std::cout, packages, pacman_);

  return 0;
}

int Auracle::Search(const std::vector<std::string>& args,
                    const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  std::vector<std::regex> patterns;
  for (const auto& arg : args) {
    try {
      patterns.emplace_back(arg, std::regex::icase | std::regex::optimize);
    } catch (const std::regex_error& e) {
      std::cerr << "error: invalid regex: " << arg << "\n";
      return -EINVAL;
    }
  }

  const auto matches = [&patterns, &options](const aur::Package& p) -> bool {
    return std::all_of(patterns.begin(), patterns.end(),
                       [&p, &options](const std::regex& re) {
                         switch (options.search_by) {
                           case aur::SearchRequest::SearchBy::NAME:
                             return std::regex_search(p.name, re);
                           case aur::SearchRequest::SearchBy::NAME_DESC:
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
  for (const auto& arg : args) {
    auto frag = GetSearchFragment(arg, options.allow_regex);
    if (frag.empty()) {
      std::cerr << "error: search string '" << arg
                << "' insufficient for searching by regular expression.\n";
      return -EINVAL;
    }

    aur_.QueueRpcRequest(aur::SearchRequest(options.search_by, frag),
                         [&](aur::ResponseWrapper<aur::RpcResponse> response) {
                           if (RpcResponseIsFailure(response)) {
                             return -EIO;
                           }

                           auto results = std::move(response.value().results);
                           std::copy_if(
                               std::make_move_iterator(results.begin()),
                               std::make_move_iterator(results.end()),
                               std::back_inserter(packages), matches);
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

int Auracle::Clone(const std::vector<std::string>& args,
                   const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  if (!ChdirIfNeeded(options.directory)) {
    return -EINVAL;
  }

  int ret = 0;
  PackageIterator iter(options.recurse, [this, &ret](const aur::Package& p) {
    aur_.QueueCloneRequest(
        aur::CloneRequest(p.pkgbase),
        [&ret, pkgbase{p.pkgbase}](
            aur::ResponseWrapper<aur::CloneResponse> response) {
          if (response.ok()) {
            std::cout << response.value().operation << " complete: "
                      << (fs::current_path() / pkgbase).string() << "\n";
          } else {
            std::cerr << "error: clone failed for " << pkgbase << ": "
                      << response.error() << "\n";
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

  if (iter.package_cache.empty()) {
    return -ENOENT;
  }

  return ret;
}

int Auracle::Download(const std::vector<std::string>& args,
                      const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  if (!ChdirIfNeeded(options.directory)) {
    return -EINVAL;
  }

  PackageIterator iter(options.recurse, [this](const aur::Package& p) {
    aur_.QueueTarballRequest(
        aur::RawRequest(aur::RawRequest::UrlForTarball(p)),
        [pkgbase{p.pkgbase}](aur::ResponseWrapper<aur::RawResponse> response) {
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

  if (iter.package_cache.empty()) {
    return -ENOENT;
  }

  return 0;
}

int Auracle::Show(const std::vector<std::string>& args,
                  const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  int resultcount = 0;
  aur_.QueueRpcRequest(
      aur::InfoRequest(args),
      [&](aur::ResponseWrapper<aur::RpcResponse> response) {
        if (RpcResponseIsFailure(response)) {
          return -EIO;
        }

        resultcount = response.value().resultcount;

        const bool print_header = resultcount > 1;

        for (const auto& pkg : response.value().results) {
          aur_.QueueRawRequest(
              aur::RawRequest(
                  aur::RawRequest::UrlForSourceFile(pkg, options.show_file)),
              [&options, print_header, pkgbase{pkg.pkgbase}](
                  aur::ResponseWrapper<aur::RawResponse> response) {
                if (!response.ok()) {
                  std::cerr << "error: request failed: " << response.error()
                            << "\n";
                  return -EIO;
                }

                switch (response.status()) {
                  case 200:
                    break;
                  case 404:
                    std::cerr << "error: file '" << options.show_file
                              << "' not found for package '" << pkgbase
                              << "'\n";
                    return -ENOENT;
                  default:
                    std::cerr << "error: unexpected HTTP response "
                              << response.status() << "\n";
                    return -EIO;
                }

                if (print_header) {
                  std::cout << "### BEGIN " << pkgbase << "/"
                            << options.show_file << "\n";
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

int Auracle::BuildOrder(const std::vector<std::string>& args,
                        const CommandOptions&) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  PackageIterator iter(/* recurse = */ true, nullptr);
  IteratePackages(args, &iter);

  int r = aur_.Wait();
  if (r < 0) {
    return r;
  }

  if (iter.package_cache.empty()) {
    return -ENOENT;
  }

  std::vector<const aur::Package*> total_ordering;
  std::unordered_set<std::string> seen;
  for (const auto& arg : args) {
    iter.package_cache.WalkDependencies(
        arg, [&total_ordering, &seen](const std::string& pkgname,
                                      const aur::Package* package) {
          if (seen.emplace(pkgname).second && package != nullptr) {
            total_ordering.push_back(package);
          }
        });
  }

  for (const auto& p : total_ordering) {
    if (pacman_->DependencyIsSatisfied(p->name)) {
      continue;
    }

    std::cout << "BUILD " << p->name << "\n";
  }

  return 0;
}

int Auracle::Sync(const std::vector<std::string>& args,
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
      info_request, [&](aur::ResponseWrapper<aur::RpcResponse> response) {
        if (RpcResponseIsFailure(response)) {
          return -EIO;
        }

        for (const auto& r : response.value().results) {
          auto iter = std::find_if(
              foreign_pkgs.cbegin(), foreign_pkgs.cend(),
              [&r](const Pacman::Package& p) { return p.pkgname == r.name; });
          if (Pacman::Vercmp(r.version, iter->pkgver) > 0) {
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

int Auracle::RawSearch(const std::vector<std::string>& args,
                       const CommandOptions& options) {
  for (const auto& arg : args) {
    aur_.QueueRawRequest(
        aur::SearchRequest(options.search_by, arg),
        [](aur::ResponseWrapper<aur::RawResponse> response) {
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

int Auracle::RawInfo(const std::vector<std::string>& args,
                     const CommandOptions&) {
  aur_.QueueRawRequest(
      aur::InfoRequest(args),
      [](aur::ResponseWrapper<aur::RawResponse> response) {
        if (!response.ok()) {
          std::cerr << "error: request failed: " << response.error() << "\n";
          return -EIO;
        }

        std::cout << response.value().bytes << "\n";
        return 0;
      });

  return aur_.Wait();
}

}  // namespace auracle

/* vim: set et ts=2 sw=2: */
