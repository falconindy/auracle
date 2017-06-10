#ifndef INMEMORY_REPO_HH
#define INMEMORY_REPO_HH

#include <map>
#include <unordered_set>

#include "aur/package.hh"

namespace dlr {

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

  using BuildList = std::vector<const aur::Package*>;

  BuildList BuildOrder(const std::string& name) const;

 private:
  void Walk(const std::string& name, BuildList* order,
            std::unordered_set<std::string>* visited) const;

  std::vector<aur::Package> packages_;

  // We store integer indicies into the packages_ vector above rather than
  // pointers to the packages. This allows the vector to resize and not
  // invalidate our index maps.
  std::map<std::string, int> index_by_pkgname_;
  std::map<std::string, int> index_by_pkgbase_;
};

}  // namespace dlr

#endif  // INMEMORY_REPO_HH
