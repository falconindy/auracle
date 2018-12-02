#include "package_cache.hh"

#include <algorithm>
#include <unordered_set>

namespace auracle {

std::pair<const aur::Package*, bool> PackageCache::AddPackage(
    aur::Package package) {
  const auto iter = std::find(packages_.begin(), packages_.end(), package);
  if (iter != packages_.cend()) {
    return {&*iter, false};
  }

  const auto& p = packages_.emplace_back(std::move(package));
  index_by_pkgbase_.emplace(p.pkgbase, packages_.size() - 1);
  index_by_pkgname_.emplace(p.name, packages_.size() - 1);

  return {&p, true};
}

const aur::Package* PackageCache::LookupByPkgname(
    const std::string& pkgname) const {
  const auto iter = index_by_pkgname_.find(pkgname);
  return iter == index_by_pkgname_.end() ? nullptr : &packages_[iter->second];
}

const aur::Package* PackageCache::LookupByPkgbase(
    const std::string& pkgbase) const {
  const auto iter = index_by_pkgbase_.find(pkgbase);
  return iter == index_by_pkgbase_.end() ? nullptr : &packages_[iter->second];
}

void PackageCache::WalkDependencies(const std::string& name,
                                    WalkDependenciesFn cb) const {
  std::unordered_set<std::string> visited;

  std::function<void(std::string)> walk;
  walk = [this, &visited, &walk, &cb](const std::string& pkgname) {
    if (!visited.insert(pkgname).second) {
      return;
    }

    const auto pkg = LookupByPkgname(pkgname);
    if (pkg != nullptr) {
      for (const auto* deplist :
           {&pkg->depends, &pkg->makedepends, &pkg->checkdepends}) {
        for (const auto& dep : *deplist) {
          walk(dep.name);
        }
      }
    }

    cb(pkgname, pkg);
  };

  walk(name);
}

}  // namespace auracle
