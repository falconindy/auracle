// SPDX-License-Identifier: MIT
#include "auracle.hh"

#include <cerrno>
#include <filesystem>
#include <functional>
#include <iostream>
#include <regex>
#include <string_view>
#include <tuple>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "aur/response.hh"
#include "auracle/dependency.hh"
#include "auracle/format.hh"
#include "auracle/pacman.hh"
#include "auracle/search_fragment.hh"
#include "auracle/sort.hh"

namespace fs = std::filesystem;

using SearchBy = aur::SearchRequest::SearchBy;

namespace auracle {

namespace {

// ResponseMerger allows us to associate multiple RPC requests and issue
// a callback only after they all succeed.
class ResponseMerger {
 public:
  using MergedResultCallback =
      absl::AnyInvocable<void(absl::StatusOr<std::vector<aur::Package>>) &&>;

  static ResponseMerger* New(MergedResultCallback&& callback) {
    return new ResponseMerger(std::move(callback));
  }

  ~ResponseMerger() {
    if (status_.ok()) {
      std::move(callback_)(std::move(packages_));
    } else {
      std::move(callback_)(status_);
    }
  }

  aur::Client::RpcResponseCallback callback() {
    ++inflight_calls_;

    return [this](absl::StatusOr<aur::RpcResponse> response) {
      status_.Update(response.status());
      if (status_.ok()) {
        absl::c_move(response->packages, std::back_inserter(packages_));
      }

      if (--inflight_calls_ == 0) {
        delete this;
      }

      return 0;
    };
  }

 private:
  ResponseMerger(MergedResultCallback callback)
      : callback_(std::move(callback)) {}

  MergedResultCallback callback_;
  int inflight_calls_ = 0;

  absl::Status status_;
  std::vector<aur::Package> packages_;
};

int ErrorNotEnoughArgs() {
  std::cerr << "error: not enough arguments.\n";
  return -EINVAL;
}

void FormatLong(const std::vector<aur::Package>& packages,
                const auracle::Pacman* pacman) {
  for (const auto& p : packages) {
    format::Long(p, pacman->GetLocalPackage(p.name));
  }
}

void FormatNameOnly(const std::vector<aur::Package>& packages) {
  for (const auto& p : packages) {
    format::NameOnly(p);
  }
}

void FormatShort(const std::vector<aur::Package>& packages,
                 const auracle::Pacman* pacman) {
  for (const auto& p : packages) {
    format::Short(p, pacman->GetLocalPackage(p.name));
  }
}

void FormatCustom(const std::vector<aur::Package>& packages,
                  const std::string& format) {
  for (const auto& p : packages) {
    format::Custom(format, p);
  }
}

void SortUnique(std::vector<aur::Package>& packages,
                const sort::Sorter& sorter) {
  absl::c_sort(packages, sorter);
  packages.resize(std::unique(packages.begin(), packages.end()) -
                  packages.begin());
}

std::vector<std::string> NotFoundPackages(
    const std::vector<std::string>& want, const std::vector<aur::Package>& got,
    const auracle::PackageCache& package_cache) {
  std::vector<std::string> missing;

  for (const auto& name : want) {
    if (package_cache.LookupByPkgname(name) != nullptr) {
      continue;
    }

    if (absl::c_find_if(got, [&](const aur::Package& pkg) {
          return pkg.name == name;
        }) != got.end()) {
      continue;
    }

    missing.push_back(name);
  }

  return missing;
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

bool RpcResponseIsFailure(const absl::StatusOr<aur::RpcResponse>& response) {
  if (response.ok()) {
    return false;
  }

  std::cerr << "error: " << response.status() << '\n';
  return true;
}

}  // namespace

Auracle::Auracle(Options options)
    : client_(aur::Client::New(aur::Client::Options()
                                   .set_baseurl(options.aur_baseurl)
                                   .set_useragent("Auracle/" PROJECT_VERSION))),
      pacman_(options.pacman) {}

void Auracle::ResolveMany(const std::vector<std::string>& depstrings,
                          aur::Client::RpcResponseCallback callback) {
  // A naive implementation of ResolveMany could be just calling search+info in
  // a loop, but we make this more complicated such that for N arguments, we can
  // issue N search requests and a single info request, rather than as many as
  // N*2 requests.

  auto deps = std::make_shared<std::vector<Dependency>>();

  auto* merger = ResponseMerger::New(
      [this, deps, callback = std::move(callback)](
          absl::StatusOr<std::vector<aur::Package>> packages) mutable {
        if (!packages.ok() || packages->empty()) {
          return std::move(callback)(packages);
        }

        aur::InfoRequest info_request;
        for (const auto& pkg : *packages) {
          info_request.AddArg(pkg.name);
        }

        client_->QueueRpcRequest(
            info_request,
            [&, callback = std::move(callback),
             deps](absl::StatusOr<aur::RpcResponse> info_response) mutable {
              if (info_response.ok()) {
                std::erase_if(info_response->packages,
                              [&](const aur::Package& package) {
                                for (const auto& dep : *deps) {
                                  if (dep.SatisfiedBy(package)) {
                                    return false;
                                  }
                                }
                                return true;
                              });
              }

              return std::move(callback)(std::move(info_response));
            });

        return 0;
      });

  for (const auto& depstring : depstrings) {
    client_->QueueRpcRequest(
        aur::SearchRequest(SearchBy::PROVIDES,
                           deps->emplace_back(depstring).name()),
        merger->callback());
  }
}

void Auracle::IteratePackages(std::vector<std::string> args,
                              Auracle::PackageIterator* state) {
  std::erase_if(args, [&](const std::string& arg) {
    return state->package_cache.LookupByPkgname(arg) != nullptr;
  });
  if (args.empty()) {
    return;
  }

  aur::InfoRequest info_request(args);
  client_->QueueRpcRequest(
      info_request, [this, state, want = std::move(args)](
                        absl::StatusOr<aur::RpcResponse> response) {
        if (RpcResponseIsFailure(response)) {
          return -EIO;
        }

        auto& results = response.value().packages;

        for (const auto& p :
             NotFoundPackages(want, results, state->package_cache)) {
          if (!pacman_->HasPackage(p)) {
            std::cerr << "no results found for " << p << "\n";
          }
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

            for (auto kind : state->resolve_depends) {
              for (const auto& dep : GetDependenciesByKind(p, kind)) {
                alldeps.push_back(Dependency(dep).name());
              }
            }

            if (alldeps.empty()) {
              return 0;
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
  client_->QueueRpcRequest(aur::InfoRequest(args),
                           [&](absl::StatusOr<aur::RpcResponse> response) {
                             if (RpcResponseIsFailure(response)) {
                               return -EIO;
                             }

                             using std::swap;
                             swap(packages, response.value().packages);

                             return 0;
                           });

  auto r = client_->Wait();
  if (r < 0) {
    return r;
  }

  if (packages.empty()) {
    return -ENOENT;
  }

  // It's unlikely, but still possible that the results may not be unique when
  // our query is large enough that it needs to be split into multiple requests.
  SortUnique(packages, options.sorter);

  if (!options.format.empty()) {
    FormatCustom(packages, options.format);
  } else {
    FormatLong(packages, pacman_);
  }

  return 0;
}

int Auracle::Resolve(const std::vector<std::string>& args,
                     const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  std::vector<aur::Package> providers;

  ResolveMany(args, [&](absl::StatusOr<aur::RpcResponse> response) {
    if (RpcResponseIsFailure(response)) {
      return -EIO;
    }

    absl::c_move(response->packages, std::back_inserter(providers));
    return 0;
  });

  int r = client_->Wait();
  if (r < 0) {
    return r;
  }

  SortUnique(providers, options.sorter);

  if (!options.format.empty()) {
    FormatCustom(providers, options.format);
  } else if (options.quiet) {
    FormatNameOnly(providers);
  } else {
    FormatShort(providers, pacman_);
  }

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

  const auto matches = [&](const aur::Package& p) {
    return absl::c_all_of(patterns, [&](const std::regex& re) {
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

  // 'name' and 'name-desc' are the only dimensions where the AUR allows
  // substring matching, so that's the only case where we're able to provide
  // something resembling regex support.
  const bool allow_regex =
      options.allow_regex && (options.search_by == SearchBy::NAME ||
                              options.search_by == SearchBy::NAME_DESC);

  std::vector<aur::Package> packages;
  for (const auto& arg : args) {
    std::string_view frag = arg;
    if (allow_regex) {
      frag = GetSearchFragment(arg);
      if (frag.empty()) {
        std::cerr << "error: search string '" << arg
                  << "' insufficient for searching by regular expression.\n";
        return -EINVAL;
      }
    }

    client_->QueueRpcRequest(aur::SearchRequest(options.search_by, frag),
                             [&](absl::StatusOr<aur::RpcResponse> response) {
                               if (RpcResponseIsFailure(response)) {
                                 return -EIO;
                               }

                               auto& results = response.value().packages;
                               std::copy_if(
                                   std::make_move_iterator(results.begin()),
                                   std::make_move_iterator(results.end()),
                                   std::back_inserter(packages), matches);
                               return 0;
                             });
  }

  int r = client_->Wait();
  if (r < 0) {
    return r;
  }

  SortUnique(packages, options.sorter);

  if (!options.format.empty()) {
    FormatCustom(packages, options.format);
  } else if (options.quiet) {
    FormatNameOnly(packages);
  } else {
    FormatShort(packages, pacman_);
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
  PackageIterator iter(
      options.recurse, options.resolve_depends,
      [this, &ret](const aur::Package& p) {
        client_->QueueCloneRequest(
            aur::CloneRequest(p.pkgbase),
            [&ret,
             pkgbase = p.pkgbase](absl::StatusOr<aur::CloneResponse> response) {
              if (response.ok()) {
                std::cout << response.value().operation << " complete: "
                          << (fs::current_path() / pkgbase).string() << "\n";
              } else {
                std::cerr << "error: clone failed for " << pkgbase << ": "
                          << response.status() << "\n";
                ret = -EIO;
              }
              return 0;
            });
      });

  IteratePackages(args, &iter);

  int r = client_->Wait();
  if (r < 0) {
    return r;
  }

  if (iter.package_cache.empty()) {
    return -ENOENT;
  }

  return ret;
}

int Auracle::Show(const std::vector<std::string>& args,
                  const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  int resultcount = 0;
  client_->QueueRpcRequest(
      aur::InfoRequest(args), [&](absl::StatusOr<aur::RpcResponse> response) {
        if (RpcResponseIsFailure(response)) {
          return -EIO;
        }

        resultcount = response.value().packages.size();

        for (const auto& pkg : response.value().packages) {
          client_->QueueRawRequest(
              aur::RawRequest::ForSourceFile(pkg, options.show_file),
              [&options, print_header = resultcount > 1, pkgbase = pkg.pkgbase](
                  absl::StatusOr<aur::RawResponse> response) {
                if (absl::IsNotFound(response.status())) {
                  std::cerr << "error: file '" << options.show_file
                            << "' not found for package '" << pkgbase << "'\n";
                  return -ENOENT;
                } else if (!response.ok()) {
                  std::cerr << "error: request failed: " << response.status()
                            << "\n";
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

  int r = client_->Wait();
  if (r < 0) {
    return r;
  }

  if (resultcount == 0) {
    return -ENOENT;
  }

  return 0;
}

int Auracle::BuildOrder(const std::vector<std::string>& args,
                        const CommandOptions& options) {
  if (args.empty()) {
    return ErrorNotEnoughArgs();
  }

  PackageIterator iter(/* recurse = */ true, options.resolve_depends, nullptr);
  IteratePackages(args, &iter);

  int r = client_->Wait();
  if (r < 0) {
    return r;
  }

  if (iter.package_cache.empty()) {
    return -ENOENT;
  }

  std::vector<
      std::tuple<std::string, const aur::Package*, std::vector<std::string>>>
      total_ordering;
  absl::flat_hash_set<std::string> seen;
  for (const auto& arg : args) {
    iter.package_cache.WalkDependencies(
        arg,
        [&](const Dependency& dep, const aur::Package* package,
            const std::vector<std::string>& dependency_path) {
          if (seen.emplace(dep.name()).second) {
            total_ordering.emplace_back(dep.name(), package, dependency_path);
          }
        },
        options.resolve_depends);
  }

  for (const auto& [name, pkg, dependency_path] : total_ordering) {
    const bool satisfied = pacman_->DependencyIsSatisfied(name);
    const bool from_aur = pkg != nullptr;
    const bool unknown = !from_aur && !pacman_->HasPackage(name);
    const bool is_target = absl::c_find(args, name) != args.end();

    if (unknown) {
      std::cout << "UNKNOWN";
      r = -ENXIO;
    } else {
      if (is_target) {
        std::cout << "TARGET";
      } else if (satisfied) {
        std::cout << "SATISFIED";
      }

      if (from_aur) {
        std::cout << "AUR";
      } else {
        std::cout << "REPOS";
      }
    }

    if (unknown) {
      for (auto iter = dependency_path.crbegin();
           iter != dependency_path.crend(); ++iter) {
        std::cout << " " << *iter;
      }
    } else {
      std::cout << " " << name;
      if (from_aur) {
        std::cout << " " << pkg->pkgbase;
      }
    }

    std::cout << "\n";
  }

  return r;
}

int Auracle::Update(const std::vector<std::string>& args,
                    const CommandOptions& options) {
  if (!ChdirIfNeeded(options.directory)) {
    return -EINVAL;
  }

  std::vector<aur::Package> packages;
  auto r = GetOutdatedPackages(args, &packages);
  if (r < 0) {
    return r;
  }

  if (packages.empty()) {
    return -ENOENT;
  }

  int ret = 0;
  PackageIterator iter(
      options.recurse, options.resolve_depends, [&](const aur::Package& p) {
        client_->QueueCloneRequest(
            aur::CloneRequest(p.pkgbase),
            [&ret,
             pkgbase = p.pkgbase](absl::StatusOr<aur::CloneResponse> response) {
              if (response.ok()) {
                std::cout << response.value().operation << " complete: "
                          << (fs::current_path() / pkgbase).string() << "\n";
              } else {
                std::cerr << "error: clone failed for " << pkgbase << ": "
                          << response.status() << "\n";
                ret = -EIO;
              }
              return 0;
            });
      });

  std::vector<std::string> outdated;
  outdated.reserve(packages.size());
  for (const auto& p : packages) {
    outdated.push_back(p.name);
  }

  IteratePackages(outdated, &iter);

  return client_->Wait();
}

int Auracle::GetOutdatedPackages(const std::vector<std::string>& args,
                                 std::vector<aur::Package>* packages) {
  aur::InfoRequest info_request;

  auto local_pkgs = pacman_->LocalPackages();
  for (const auto& pkg : local_pkgs) {
    if (args.empty() || absl::c_find(args, pkg.pkgname) != args.end()) {
      info_request.AddArg(pkg.pkgname);
    }
  }

  client_->QueueRpcRequest(
      info_request, [&](absl::StatusOr<aur::RpcResponse> response) {
        if (RpcResponseIsFailure(response)) {
          return -EIO;
        }

        auto& results = response.value().packages;
        std::copy_if(std::make_move_iterator(results.begin()),
                     std::make_move_iterator(results.end()),
                     std::back_inserter(*packages), [&](const aur::Package& p) {
                       auto local = pacman_->GetLocalPackage(p.name);

                       return local &&
                              Pacman::Vercmp(p.version, local->pkgver) > 0;
                     });

        return 0;
      });

  return client_->Wait();
}

int Auracle::Outdated(const std::vector<std::string>& args,
                      const CommandOptions& options) {
  std::vector<aur::Package> packages;

  auto r = GetOutdatedPackages(args, &packages);
  if (r < 0) {
    return r;
  }

  if (packages.empty()) {
    return -ENOENT;
  }

  // Not strictly needed, but let's keep output order stable
  std::sort(packages.begin(), packages.end(),
            sort::MakePackageSorter("name", sort::OrderBy::ORDER_ASC));

  for (const auto& r : packages) {
    if (options.quiet) {
      format::NameOnly(r);
    } else {
      auto local = pacman_->GetLocalPackage(r.name);
      format::Update(*local, r);
    }
  }

  return 0;
}

namespace {

int RawRequestDone(absl::StatusOr<aur::RawResponse> response) {
  if (!response.ok()) {
    std::cerr << "error: request failed: " << response.status() << "\n";
    return -EIO;
  }

  std::cout << response.value().bytes << "\n";
  return 0;
}

}  // namespace

int Auracle::RawSearch(const std::vector<std::string>& args,
                       const CommandOptions& options) {
  for (const auto& arg : args) {
    client_->QueueRawRequest(aur::SearchRequest(options.search_by, arg),
                             &RawRequestDone);
  }

  return client_->Wait();
}

int Auracle::RawInfo(const std::vector<std::string>& args,
                     const CommandOptions&) {
  client_->QueueRawRequest(aur::InfoRequest(args), &RawRequestDone);

  return client_->Wait();
}

}  // namespace auracle

/* vim: set et ts=2 sw=2: */
