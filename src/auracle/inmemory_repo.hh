#ifndef INMEMORY_REPO_HH
#define INMEMORY_REPO_HH

#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "aur/package.hh"

namespace auracle {

class InMemoryRepo {
 public:
  InMemoryRepo() {}
  ~InMemoryRepo() {}

  InMemoryRepo(const InMemoryRepo&) = delete;
  InMemoryRepo& operator=(const InMemoryRepo&) = delete;

  InMemoryRepo(InMemoryRepo&&) = default;
  InMemoryRepo& operator=(InMemoryRepo&&) = default;

  std::pair<const aur::Package*, bool> AddPackage(aur::Package package);

  const aur::Package* LookupByPkgname(const std::string& pkgname) const;
  const aur::Package* LookupByPkgbase(const std::string& pkgbase) const;

  int size() const { return packages_.size(); }

  bool empty() const { return size() == 0; }

  void WalkDependencies(const std::string& name,
                        std::function<void(const aur::Package*)> cb) const;

 private:
  std::vector<aur::Package> packages_;

  // We store integer indicies into the packages_ vector above rather than
  // pointers to the packages. This allows the vector to resize and not
  // invalidate our index maps.
  std::unordered_map<std::string, int> index_by_pkgname_;
  std::unordered_map<std::string, int> index_by_pkgbase_;
};

}  // namespace auracle

#endif  // INMEMORY_REPO_HH
