#include "dependency_kind.hh"

#include <stdexcept>

namespace auracle {

bool ParseDependencyKinds(std::string_view input,
                          std::set<DependencyKind>* kinds) {
  if (input.empty()) {
    return true;
  }

  bool remove = false;
  bool append = false;

  switch (input[0]) {
    case '^':
    case '!':
      remove = true;
      input.remove_prefix(1);
      break;
    case '+':
      append = true;
      input.remove_prefix(1);
      break;
  }

  std::set<DependencyKind> parsed_kinds;
  while (input.size() > 0) {
    auto end = input.find(',');
    if (end == 0) {
      input.remove_prefix(1);
      continue;
    } else if (end == input.npos) {
      end = input.size();
    }

    std::string_view dep_kind = input.substr(0, end);
    if (dep_kind == "depends") {
      parsed_kinds.insert(DependencyKind::Depend);
    } else if (dep_kind == "checkdepends") {
      parsed_kinds.insert(DependencyKind::CheckDepend);
    } else if (dep_kind == "makedepends") {
      parsed_kinds.insert(DependencyKind::MakeDepend);
    } else {
      return false;
    }

    input.remove_prefix(dep_kind.size());
  }

  if (remove) {
    for (auto kind : parsed_kinds) {
      kinds->erase(kind);
    }
  } else if (append) {
    kinds->insert(parsed_kinds.begin(), parsed_kinds.end());
  } else {
    kinds->swap(parsed_kinds);
  }

  return true;
}

const std::vector<aur::Dependency>& GetDependenciesByKind(
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
