// SPDX-License-Identifier: MIT
#include "dependency_kind.hh"

#include <stdexcept>

#include "absl/strings/str_split.h"

namespace auracle {

bool ParseDependencyKinds(std::string_view input,
                          absl::btree_set<DependencyKind>* kinds) {
  if (input.empty()) {
    return true;
  }

  enum : int8_t {
    MODE_OVERWRITE,
    MODE_REMOVE,
    MODE_APPEND,
  } mode = MODE_OVERWRITE;

  switch (input[0]) {
    case '^':
    case '!':
      input.remove_prefix(1);
      mode = MODE_REMOVE;
      break;
    case '+':
      input.remove_prefix(1);
      mode = MODE_APPEND;
      break;
  }

  absl::btree_set<DependencyKind> parsed_kinds;
  for (const auto kind : absl::StrSplit(input, ',')) {
    if (kind == "depends") {
      parsed_kinds.insert(DependencyKind::Depend);
    } else if (kind == "checkdepends") {
      parsed_kinds.insert(DependencyKind::CheckDepend);
    } else if (kind == "makedepends") {
      parsed_kinds.insert(DependencyKind::MakeDepend);
    } else {
      return false;
    }
  }

  switch (mode) {
    case MODE_OVERWRITE:
      *kinds = std::move(parsed_kinds);
      break;
    case MODE_REMOVE:
      for (auto kind : parsed_kinds) {
        kinds->erase(kind);
      }
      break;
    case MODE_APPEND:
      kinds->merge(parsed_kinds);
      break;
  }

  return true;
}

const std::vector<std::string>& GetDependenciesByKind(
    const aur::Package* package, DependencyKind kind) {
  switch (kind) {
    case DependencyKind::Depend:
      return package->depends;
    case DependencyKind::MakeDepend:
      return package->makedepends;
    case DependencyKind::CheckDepend:
      return package->checkdepends;
  }

  throw std::logic_error("unhandled DependencyKind");
}

}  // namespace auracle
