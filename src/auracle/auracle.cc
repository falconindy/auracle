// SPDX-License-Identifier: MIT
#include "auracle.hh"

#include <cerrno>
#include <filesystem>
#include <print>
#include <regex>
#include <string_view>
#include <tuple>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
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
  std::println(stderr, "error: not enough arguments.");
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

bool ChdirIfNeeded(const fs::path& target) {
  if (target.empty()) {
    return true;
  }

  std::error_code ec;
  fs::current_path(target, ec);
  if (ec.value() != 0) {
    std::println(stderr, "error: failed to change directory to {}: {}",
                 target.string(), ec.message());
    return false;
  }

  return true;
}

bool RpcResponseIsFailure(const absl::StatusOr<aur::RpcResponse>& response) {
  if (response.ok()) {
    return false;
  }

  std::println(stderr, "error: {}", response.status().ToString());
  return true;
}

}  // namespace

Auracle::Auracle(Options options)
    : client_(aur::Client::New(aur::Client::Options()
                                   .set_baseurl(options.baseurl)
                                   .set_proxy(options.proxy)
                                   .set_useragent("Auracle/" PROJECT_VERSION))),
      pacman_(options.pacman) {}

void Auracle::ResolveDependencies(
    std::shared_ptr<std::vector<Dependency>> deps,
    absl::AnyInvocable<void(absl::StatusOr<std::vector<aur::Package>>) &&>
        callback) {
  // A naive implementation could be just calling search+info in a loop, but
  // we make this more complicated such that for N arguments, we can issue N
  // search requests and a single info request, rather than as many as N*2
  // requests.

  auto* merger = ResponseMerger::New(
      [this, deps, callback = std::move(callback)](
          absl::StatusOr<std::vector<aur::Package>> packages) mutable {
        if (!packages.ok() || packages->empty()) {
          std::move(callback)(std::move(packages));
          return;
        }

        client_->QueueRpcRequest(
            aur::InfoRequest(*packages),
            [callback = std::move(callback),
             deps](absl::StatusOr<aur::RpcResponse> info_response) mutable {
              if (!info_response.ok()) {
                std::move(callback)(info_response.status());
                return -EIO;
              }

              auto packages = std::move(info_response->packages);
              std::erase_if(packages, [&](const aur::Package& package) {
                for (const auto& dep : *deps) {
                  if (dep.SatisfiedBy(package)) {
                    return false;
                  }
                }
                return true;
              });

              std::move(callback)(std::move(packages));
              return 0;
            });
      });

  for (const auto& dep : *deps) {
    client_->QueueRpcRequest(aur::SearchRequest(SearchBy::PROVIDES, dep.name()),
                             merger->callback());
  }
}

void Auracle::ResolveMany(const std::vector<std::string>& depstrings,
                          aur::Client::RpcResponseCallback callback) {
  auto deps = std::make_shared<std::vector<Dependency>>();
  deps->reserve(depstrings.size());
  for (const auto& depstring : depstrings) {
    deps->emplace_back(depstring);
  }

  ResolveDependencies(
      std::move(deps),
      [callback = std::move(callback)](
          absl::StatusOr<std::vector<aur::Package>> packages) mutable {
        std::move(callback)(
            packages.ok()
                ? absl::StatusOr<aur::RpcResponse>(
                      aur::RpcResponse(*std::move(packages)))
                : absl::StatusOr<aur::RpcResponse>(packages.status()));
      });
}

void Auracle::CacheAndMaybeRecurse(aur::Package package,
                                   Auracle::PackageIterator* state,
                                   bool descend) {
  // check for the pkgbase existing in our repo
  const bool have_pkgbase =
      state->package_cache.LookupByPkgbase(package.pkgbase) != nullptr;

  // Regardless, try to add the package, as it might be another member
  // of the same pkgbase.
  auto [p, added] = state->package_cache.AddPackage(std::move(package));

  if (!added || have_pkgbase || !descend) {
    return;
  }

  if (state->callback) {
    state->callback(*p);
  }

  if (state->recurse) {
    std::vector<Dependency> alldeps;
    alldeps.reserve(p->depends.size() + p->makedepends.size() +
                    p->checkdepends.size());

    // The same depstring can legitimately appear under more than one kind
    // (e.g. a dependency listed in both Depends and MakeDepends) -- dedupe
    // so it's only resolved once. Left unfiltered, a duplicate here means a
    // duplicated search-by-provides request downstream in
    // ResolveDependencies, and the real AUR (unlike the test fakeserver)
    // doesn't dedupe repeated arg[] values, so that shows up as a
    // false-positive "multiple providers" ambiguity.
    absl::flat_hash_set<std::string> seen_depstrings;
    for (auto kind : state->resolve_depends) {
      for (const auto& dep : GetDependenciesByKind(p, kind)) {
        if (seen_depstrings.insert(dep).second) {
          alldeps.emplace_back(dep);
        }
      }
    }

    if (!alldeps.empty()) {
      IteratePackages(std::move(alldeps), state);
    }
  }
}

void Auracle::AddPackagesAndRecurse(std::vector<aur::Package>& results,
                                    Auracle::PackageIterator* state) {
  for (auto& result : results) {
    CacheAndMaybeRecurse(std::move(result), state, /*descend=*/true);
  }
}

void Auracle::IteratePackages(std::vector<std::string> args,
                              Auracle::PackageIterator* state) {
  std::vector<Dependency> deps;
  deps.reserve(args.size());
  for (auto& arg : args) {
    deps.emplace_back(std::move(arg));
  }

  IteratePackages(std::move(deps), state);
}

void Auracle::IteratePackages(std::vector<Dependency> deps,
                              Auracle::PackageIterator* state) {
  // Drop anything already satisfied by a package we've already fetched,
  // either by an exact name match or via a (possibly versioned) provides.
  std::erase_if(deps, [&](const Dependency& dep) {
    if (const auto* pkg = state->package_cache.LookupByPkgname(dep.name());
        pkg != nullptr && dep.SatisfiedBy(*pkg)) {
      return true;
    }

    return !state->package_cache.FindDependencySatisfiers(dep).empty();
  });
  if (deps.empty()) {
    return;
  }

  std::vector<std::string> names;
  names.reserve(deps.size());
  for (const auto& dep : deps) {
    names.push_back(dep.name());
  }

  client_->QueueRpcRequest(
      aur::InfoRequest(names),
      [this, state, deps = std::move(deps)](
          absl::StatusOr<aur::RpcResponse> response) mutable {
        if (RpcResponseIsFailure(response)) {
          return -EIO;
        }

        auto& results = response.value().packages;

        // A name match doesn't necessarily mean satisfaction (the AUR
        // package by that name might be out of date or otherwise
        // incompatible with the required version) -- fall through to
        // provider resolution for anything not actually satisfied.
        auto unresolved = std::make_shared<std::vector<Dependency>>();
        for (const auto& dep : deps) {
          if (absl::c_any_of(results, [&](const aur::Package& pkg) {
                return dep.SatisfiedBy(pkg);
              })) {
            continue;
          }
          if (pacman_->HasPackage(dep.depstring())) {
            continue;
          }
          unresolved->push_back(dep);
        }

        AddPackagesAndRecurse(results, state);

        if (!unresolved->empty()) {
          ResolveDependencies(
              unresolved,
              [this, state, unresolved](
                  absl::StatusOr<std::vector<aur::Package>> resolved) mutable {
                if (!resolved.ok()) {
                  return;
                }

                // Diagnose each unresolved depstring: no candidates at all,
                // or more than one (ambiguous -- we don't guess, matching
                // buildorder's UNRESOLVED handling in WalkDependencies).
                for (const auto& dep : *unresolved) {
                  std::vector<const aur::Package*> satisfiers;
                  for (const auto& pkg : *resolved) {
                    if (dep.SatisfiedBy(pkg)) {
                      satisfiers.push_back(&pkg);
                    }
                  }

                  if (satisfiers.empty()) {
                    if (!pacman_->HasPackage(dep.depstring())) {
                      std::println(stderr, "no results found for {}",
                                   dep.depstring());
                    }
                  } else if (satisfiers.size() > 1) {
                    std::print(stderr,
                               "warning: multiple AUR packages satisfy '{}':",
                               dep.depstring());
                    for (const auto* s : satisfiers) {
                      std::print(stderr, " {}", s->name);
                    }
                    std::println(stderr, "");
                  }
                }

                // Cache every candidate (so a later WalkDependencies can
                // still see and report on ambiguous providers), but only
                // descend into a candidate's own dependencies -- and only
                // invoke the iterator callback (e.g. clone) for it -- when
                // it's the unambiguous resolution of at least one
                // dependency. This is computed as a separate pass, before
                // any moving out of *resolved below: moving a package out
                // empties it in place, and the ambiguity check for each
                // remaining candidate needs to see every candidate intact.
                absl::InlinedVector<bool, 8> descend;
                descend.reserve(resolved->size());
                for (const auto& pkg : *resolved) {
                  descend.push_back(
                      absl::c_any_of(*unresolved, [&](const Dependency& dep) {
                        if (!dep.SatisfiedBy(pkg)) {
                          return false;
                        }
                        return absl::c_count_if(
                                   *resolved,
                                   [&](const aur::Package& candidate) {
                                     return dep.SatisfiedBy(candidate);
                                   }) == 1;
                      }));
                }

                for (size_t i = 0; i < resolved->size(); ++i) {
                  CacheAndMaybeRecurse(std::move((*resolved)[i]), state,
                                       descend[i]);
                }
              });
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
      std::println(stderr, "error: invalid regex: {}", arg);
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
        std::println(stderr,
                     "error: search string '{}' insufficient for searching by "
                     "regular expression.",
                     arg);
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
                std::println("{} complete: {}", response.value().operation,
                             (fs::current_path() / pkgbase).string());
              } else {
                std::println(stderr, "error: clone failed for {}: {}", pkgbase,
                             response.status().ToString());
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
                  std::println(stderr,
                               "error: file '{}' not found for package '{}'",
                               options.show_file, pkgbase);
                  return -ENOENT;
                } else if (!response.ok()) {
                  std::println(stderr, "error: request failed: {}",
                               response.status().ToString());
                  return -EIO;
                }

                if (print_header) {
                  std::println("### BEGIN {}/{}", pkgbase, options.show_file);
                }
                std::println("{}", response.value().bytes);
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

  std::vector<std::tuple<std::string, std::string, const aur::Package*,
                         std::vector<const aur::Package*>,
                         std::vector<std::string>>>
      total_ordering;  // name, depstring, package, ambiguous_providers,
                       // dependency_path
  absl::flat_hash_set<std::string> seen;
  for (const auto& arg : args) {
    iter.package_cache.WalkDependencies(
        arg,
        [&](const Dependency& dep, const aur::Package* package,
            const std::vector<const aur::Package*>& ambiguous_providers,
            const std::vector<std::string>& dependency_path) {
          if (seen.emplace(dep.name()).second) {
            total_ordering.emplace_back(dep.name(), dep.depstring(), package,
                                        ambiguous_providers, dependency_path);
          }
        },
        options.resolve_depends);
  }

  for (const auto& [name, depstring, pkg, ambiguous_providers,
                    dependency_path] : total_ordering) {
    if (!ambiguous_providers.empty()) {
      std::print("UNRESOLVED {}", depstring);
      for (const auto* provider : ambiguous_providers) {
        std::print(" {}", provider->pkgbase);
      }
      std::println("");
      r = -ENXIO;
      continue;
    }

    const bool satisfied = pacman_->DependencyIsSatisfied(depstring);
    const bool from_aur = pkg != nullptr;
    const bool unknown = !from_aur && !pacman_->HasPackage(depstring);
    const bool is_target = absl::c_find(args, name) != args.end();

    if (unknown) {
      std::print("UNKNOWN");
      r = -ENXIO;
    } else {
      if (is_target) {
        std::print("TARGET");
      } else if (satisfied) {
        std::print("SATISFIED");
      }

      if (from_aur) {
        std::print("AUR");
      } else {
        std::print("REPOS");
      }
    }

    if (unknown) {
      for (auto iter = dependency_path.crbegin();
           iter != dependency_path.crend(); ++iter) {
        std::print(" {}", *iter);
      }
    } else {
      std::print(" {}", name);
      if (from_aur) {
        std::print(" {}", pkg->pkgbase);
      }
    }

    std::println("");
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
                std::println("{} complete: {}", response.value().operation,
                             (fs::current_path() / pkgbase).string());
              } else {
                std::println(stderr, "error: clone failed for {}: {}", pkgbase,
                             response.status().ToString());
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

  r = client_->Wait();
  return r < 0 ? r : ret;
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
    std::println(stderr, "error: request failed: {}",
                 response.status().ToString());
    return -EIO;
  }

  std::println("{}", response.value().bytes);
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
