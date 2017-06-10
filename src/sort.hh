#ifndef SORT_HH
#define SORT_HH

#include <functional>

#include "pacman.hh"

namespace sort {

#define DEFINE_COMPARATOR(ComparatorName, Field, Cmp)                     \
  struct ComparatorName {                                                 \
    bool operator()(const aur::Package& a, const aur::Package& b) const { \
      return Cmp()(a.Field, b.Field);                                     \
    }                                                                     \
    bool operator()(const aur::Package* a, const aur::Package* b) const { \
      return Cmp()(a->Field, b->Field);                                   \
    }                                                                     \
  }

DEFINE_COMPARATOR(PackageNameLess, name, std::less<std::string>);
DEFINE_COMPARATOR(PackageDescriptionLess, description, std::less<std::string>);
DEFINE_COMPARATOR(PackageMaintainerLess, maintainer, std::less<std::string>);
DEFINE_COMPARATOR(PackagePackageBaseLess, pkgbase, std::less<std::string>);
DEFINE_COMPARATOR(PackageVotesLess, votes, std::less<int>);
DEFINE_COMPARATOR(PackagePopularityLess, popularity, std::less<double>);
DEFINE_COMPARATOR(PackageModifiedTimeLess, modified_s, std::less<time_t>);
DEFINE_COMPARATOR(PackageSubmittedTimeLess, submitted_s, std::less<time_t>);
DEFINE_COMPARATOR(PackageVersionLess, version, dlr::Pacman::VersionLess);

#undef DEFINE_COMPARATOR

}  // namespace sort

#endif  // SORT_HH
