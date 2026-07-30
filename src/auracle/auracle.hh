// SPDX-License-Identifier: MIT
#ifndef AURACLE_AURACLE_HH_
#define AURACLE_AURACLE_HH_

#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "aur/client.hh"
#include "aur/request.hh"
#include "auracle/dependency.hh"
#include "auracle/dependency_kind.hh"
#include "auracle/package_cache.hh"
#include "auracle/pacman.hh"
#include "auracle/sort.hh"

namespace auracle {

class Auracle {
 public:
  struct Options {
    Options& set_baseurl(std::string baseurl) {
      this->baseurl = std::move(baseurl);
      return *this;
    }

    Options& set_proxy(std::optional<std::string> proxy) {
      this->proxy = std::move(proxy);
      return *this;
    }

    Options& set_pacman(Pacman* pacman) {
      this->pacman = pacman;
      return *this;
    }

    Options& set_quiet(bool quiet) {
      this->quiet = quiet;
      return *this;
    }

    std::string baseurl;
    std::optional<std::string> proxy;
    Pacman* pacman = nullptr;
    bool quiet = false;
  };

  explicit Auracle(Options options);

  ~Auracle() = default;

  Auracle(const Auracle&) = delete;
  Auracle& operator=(const Auracle&) = delete;

  Auracle(Auracle&&) = default;
  Auracle& operator=(Auracle&&) = default;

  struct CommandOptions {
    aur::SearchRequest::SearchBy search_by =
        aur::SearchRequest::SearchBy::NAME_DESC;
    std::string directory;
    bool recurse = false;
    bool allow_regex = true;
    bool quiet = false;
    std::string show_file = "PKGBUILD";
    sort::Sorter sorter =
        sort::MakePackageSorter("name", sort::OrderBy::ORDER_ASC);
    std::string format;
    absl::btree_set<DependencyKind> resolve_depends = {
        DependencyKind::Depend, DependencyKind::CheckDepend,
        DependencyKind::MakeDepend};
  };

  int BuildOrder(const std::vector<std::string>& args,
                 const CommandOptions& options);
  int Clone(const std::vector<std::string>& args,
            const CommandOptions& options);
  int Info(const std::vector<std::string>& args, const CommandOptions& options);
  int Resolve(const std::vector<std::string>& arg,
              const CommandOptions& options);
  int Show(const std::vector<std::string>& args, const CommandOptions& options);
  int RawInfo(const std::vector<std::string>& args,
              const CommandOptions& options);
  int RawSearch(const std::vector<std::string>& args,
                const CommandOptions& options);
  int Search(const std::vector<std::string>& args,
             const CommandOptions& options);
  int Outdated(const std::vector<std::string>& args,
               const CommandOptions& options);
  int Update(const std::vector<std::string>& args,
             const CommandOptions& options);

 private:
  struct PackageIterator {
    using PackageCallback = std::function<void(const aur::Package&)>;

    PackageIterator(bool recurse,
                    absl::btree_set<DependencyKind> resolve_depends,
                    PackageCallback callback)
        : recurse(recurse),
          resolve_depends(resolve_depends),
          callback(std::move(callback)) {}

    bool recurse;
    absl::btree_set<DependencyKind> resolve_depends;

    const PackageCallback callback;
    PackageCache package_cache;
  };

  void ResolveMany(const std::vector<std::string>& depstrings,
                   aur::Client::RpcResponseCallback callback);

  // Resolves a set of already-parsed dependencies to the AUR packages that
  // satisfy them (by name or by versioned/unversioned provides). This is the
  // single seam through which auracle asks "what satisfies these
  // depstrings?" -- if the AUR ever grows a server-side resolve/satisfier
  // endpoint, only this function's implementation needs to change.
  void ResolveDependencies(
      std::shared_ptr<std::vector<Dependency>> deps,
      absl::AnyInvocable<void(absl::StatusOr<std::vector<aur::Package>>) &&>
          callback);

  int GetOutdatedPackages(const std::vector<std::string>& args,
                          std::vector<aur::Package>* packages);

  void IteratePackages(std::vector<std::string> args, PackageIterator* state);
  void IteratePackages(std::vector<Dependency> deps, PackageIterator* state);
  void AddPackagesAndRecurse(std::vector<aur::Package>& results,
                             PackageIterator* state);
  void CacheAndMaybeRecurse(aur::Package package, PackageIterator* state,
                            bool descend);

  // Returns the subset of `deps` satisfied by neither `results` nor the
  // local pacman database -- these still need provider resolution.
  std::shared_ptr<std::vector<Dependency>> FindUnresolvedDependencies(
      const std::vector<Dependency>& deps,
      const std::vector<aur::Package>& results);

  // Handles the outcome of resolving `unresolved` via ResolveDependencies:
  // diagnoses each depstring (no candidates, or ambiguous candidates) and
  // caches/recurses into the unambiguous resolutions.
  void CacheResolvedDependencies(
      std::shared_ptr<std::vector<Dependency>> unresolved,
      PackageIterator* state,
      absl::StatusOr<std::vector<aur::Package>> resolved);

  std::unique_ptr<aur::Client> client_;
  Pacman* pacman_;
};

}  // namespace auracle

#endif  // AURACLE_AURACLE_HH_
