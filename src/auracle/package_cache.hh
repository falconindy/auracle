#ifndef PACKAGE_CACHE_HH
#define PACKAGE_CACHE_HH

#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "aur/package.hh"

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

  int size() const { return packages_.size(); }

  bool empty() const { return size() == 0; }

  using WalkDependenciesFn =
      std::function<void(const std::string& name, const aur::Package*)>;
  void WalkDependencies(const std::string& name, WalkDependenciesFn cb) const;

 private:
  std::vector<aur::Package> packages_;

  // We store integer indicies into the packages_ vector above rather than
  // pointers to the packages. This allows the vector to resize and not
  // invalidate our index maps.
  std::unordered_map<std::string, int> index_by_pkgname_;
  std::unordered_map<std::string, int> index_by_pkgbase_;
};

}  // namespace auracle

#endif  // PACKAGE_CACHE_HH
