// SPDX-License-Identifier: MIT
#ifndef AURACLE_DEPENDENCY_HH_
#define AURACLE_DEPENDENCY_HH_

#include <string>
#include <string_view>

#include "aur/package.hh"

namespace auracle {

// Dependency provides a simple interface around dependency resolution.
class Dependency final {
 public:
  // Constructs a Dependency described by the given |depstring|. A
  // depstring follows the same format as that described by libalpm.
  explicit Dependency(std::string_view depstring);

  Dependency(Dependency&&) = default;
  Dependency& operator=(Dependency&&) = default;

  Dependency(const Dependency&) = default;
  Dependency& operator=(const Dependency&) = default;

  const std::string& name() const { return name_; }

  bool is_versioned() const { return !version_.empty(); }

  // Returns true if the given candidate package satisifes the dependency
  // requirement. A dependency is satisfied if:
  //  a) The given |candidate| directly supplies the necessary name and possibly
  //     version.
  //  b) The given |candidate| offers a `provide' that can satisfy the given
  //     dependency. Unlike situation 'a', an unversioned provide can never
  //     satisfy a versioned dependency.
  bool SatisfiedBy(const aur::Package& candidate) const;

 private:
  enum class Mod {
    ANY,
    EQ,
    GE,
    GT,
    LE,
    LT,
  };

  bool SatisfiedByVersion(const std::string& version) const;

  std::string depstring_;
  std::string name_;
  std::string version_;
  Mod mod_;
};

}  // namespace auracle

#endif
