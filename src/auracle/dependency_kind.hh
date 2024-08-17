// SPDX-License-Identifier: MIT
#ifndef AURACLE_DEPENDENCY_KIND_HH_
#define AURACLE_DEPENDENCY_KIND_HH_

#include <string_view>
#include <vector>

#include "absl/container/btree_set.h"
#include "aur/package.hh"

namespace auracle {

enum class DependencyKind : int {
  Depend,
  MakeDepend,
  CheckDepend,
};

bool ParseDependencyKinds(std::string_view input,
                          absl::btree_set<DependencyKind>* dependency_kinds);

const std::vector<std::string>& GetDependenciesByKind(
    const aur::Package* package, DependencyKind kind);

}  // namespace auracle

#endif  // AURACLE_DEPENDENCY_KIND_HH_
