#include "inmemory_repo.hh"

#include <algorithm>

namespace dlr {

std::pair<const aur::Package*, bool> InMemoryRepo::AddPackage(
    aur::Package package) {
  const auto iter = std::find_if(
      packages_.cbegin(), packages_.cend(),
      [&package](const aur::Package& p) { return p.name == package.name; });
  if (iter != packages_.cend()) {
    return {&*iter, false};
  }

  const auto& p = packages_.emplace_back(std::move(package));
  index_by_pkgbase_.emplace(p.pkgbase, packages_.size() - 1);
  index_by_pkgname_.emplace(p.name, packages_.size() - 1);

  return {&p, true};
}

const aur::Package* InMemoryRepo::LookupByPkgname(
    const std::string& pkgname) const {
  const auto iter = index_by_pkgname_.find(pkgname);
  return iter == index_by_pkgname_.end() ? nullptr : &packages_[iter->second];
}

const aur::Package* InMemoryRepo::LookupByPkgbase(
    const std::string& pkgbase) const {
  const auto iter = index_by_pkgbase_.find(pkgbase);
  return iter == index_by_pkgbase_.end() ? nullptr : &packages_[iter->second];
}

void InMemoryRepo::WalkDependencies(
    const std::string& name,
    std::function<void(const aur::Package*)> cb) const {
  std::unordered_set<std::string> visited;

  std::function<void(std::string)> walk;
  walk = [this, &visited, &walk, &cb](const std::string& n) {
    if (visited.find(n) != visited.end()) {
      return;
    }

    const auto pkg = LookupByPkgname(n);
    if (pkg == nullptr) {
      return;
    }

    visited.insert(pkg->name);

    for (const auto* deplist :
         {&pkg->depends, &pkg->makedepends, &pkg->checkdepends}) {
      for (const auto& dep : *deplist) {
        walk(dep.name);
      }
    }

    cb(pkg);
  };

  walk(name);
}

}  // namespace dlr
