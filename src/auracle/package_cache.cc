#include "package_cache.hh"

#include <iostream>
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
    void MaybeReportCycle(const std::string& step) {
      auto cycle_start =
          std::find(dependency_path_.cbegin(), dependency_path_.cend(), step);
      if (cycle_start == dependency_path_.cend()) {
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
  std::unordered_set<std::string> visited;
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
