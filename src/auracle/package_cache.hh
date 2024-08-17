// SPDX-License-Identifier: MIT
#ifndef PACKAGE_AURACLE_CACHE_HH_
#define PACKAGE_AURACLE_CACHE_HH_

#include <functional>
#include <set>
#include <utility>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "aur/package.hh"
#include "auracle/dependency.hh"
#include "auracle/dependency_kind.hh"

namespace auracle {

class PackageCache {
 public:
  PackageCache() = default;
  ~PackageCache() = default;

  PackageCache(const PackageCache&) = delete;
  PackageCache& operator=(const PackageCache&) = delete;

  PackageCache(PackageCache&&) = default;
  PackageCache& operator=(PackageCache&&) = default;

  std::pair<const aur::Package*, bool> AddPackage(aur::Package package);

  const aur::Package* LookupByPkgname(const std::string& pkgname) const;
  const aur::Package* LookupByPkgbase(const std::string& pkgbase) const;
  std::vector<const aur::Package*> FindDependencySatisfiers(
      const Dependency& depstring) const;

  int size() const { return packages_.size(); }

  bool empty() const { return size() == 0; }

  using WalkDependenciesFn =
      std::function<void(const Dependency& dep, const aur::Package* package,
                         const std::vector<std::string>& dependency_path)>;
  void WalkDependencies(
      const std::string& name, WalkDependenciesFn cb,
      const absl::btree_set<DependencyKind>& dependency_kinds) const;

 private:
  std::vector<aur::Package> packages_;

  using PackageIndex = absl::flat_hash_map<std::string, int>;

  const aur::Package* LookupByIndex(const PackageIndex& index,
                                    const std::string& item) const;

  // We store integer indicies into the packages_ vector above rather than
  // pointers to the packages. This allows the vector to resize and not
  // invalidate our index maps.
  PackageIndex index_by_pkgname_;
  PackageIndex index_by_pkgbase_;
  absl::flat_hash_map<std::string, std::vector<int>> index_by_provide_;
};

}  // namespace auracle

#endif  // PACKAGE_AURACLE_CACHE_HH_
