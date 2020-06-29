// SPDX-License-Identifier: MIT
#include "package_cache.hh"

#include <iostream>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"

namespace auracle {

std::pair<const aur::Package*, bool> PackageCache::AddPackage(
    aur::Package package) {
  const auto iter = absl::c_find(packages_, package);
  if (iter != packages_.end()) {
    return {&*iter, false};
  }

  const auto& p = packages_.emplace_back(std::move(package));
  index_by_pkgbase_.emplace(p.pkgbase, packages_.size() - 1);
  index_by_pkgname_.emplace(p.name, packages_.size() - 1);

  return {&p, true};
}

const aur::Package* PackageCache::LookupByIndex(const PackageIndex& index,
                                                const std::string& item) const {
  const auto iter = index.find(item);
  return iter == index.end() ? nullptr : &packages_[iter->second];
}

const aur::Package* PackageCache::LookupByPkgname(
    const std::string& pkgname) const {
  return LookupByIndex(index_by_pkgname_, pkgname);
}

const aur::Package* PackageCache::LookupByPkgbase(
    const std::string& pkgbase) const {
  return LookupByIndex(index_by_pkgbase_, pkgbase);
}

class DependencyPath : public std::vector<std::string> {
 public:
  class Step {
   public:
    Step(DependencyPath& dependency_path, std::string step)
        : dependency_path_(dependency_path) {
      MaybeReportCycle(step);

      dependency_path_.push_back(step);
    }

    ~Step() { dependency_path_.pop_back(); }

   private:
    void MaybeReportCycle(const std::string& step) const {
      auto cycle_start = absl::c_find(dependency_path_, step);
      if (cycle_start == dependency_path_.end()) {
        return;
      }

      std::cerr << "warning: found dependency cycle:";

      // Print the path leading up to the start of the cycle
      auto iter = dependency_path_.cbegin();
      while (iter != cycle_start) {
        std::cerr << " " << *iter << " ->";
        ++iter;
      }

      // Print the cycle itself, wrapped in brackets
      std::cerr << " [ " << *iter;
      ++iter;
      while (iter != dependency_path_.cend()) {
        std::cerr << " -> " << *iter;
        ++iter;
      }

      std::cerr << " -> " << *cycle_start << " ]\n";
    }

    DependencyPath& dependency_path_;
  };
};

void PackageCache::WalkDependencies(
    const std::string& name, WalkDependenciesFn cb,
    const std::set<DependencyKind>& dependency_kinds) const {
  absl::flat_hash_set<std::string> visited;
  DependencyPath dependency_path;

  std::function<void(std::string)> walk;
  walk = [&](const std::string& pkgname) {
    DependencyPath::Step step(dependency_path, pkgname);

    if (!visited.insert(pkgname).second) {
      return;
    }

    const auto* pkg = LookupByPkgname(pkgname);
    if (pkg != nullptr) {
      for (auto kind : dependency_kinds) {
        const auto& deplist = GetDependenciesByKind(pkg, kind);
        for (const auto& dep : deplist) {
          walk(dep.name);
        }
      }
    }

    cb(pkgname, pkg, dependency_path);
  };

  walk(name);
}

}  // namespace auracle
