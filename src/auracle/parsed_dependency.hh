#ifndef AURACLE_PARSED_DEPENDENCY_H
#define AURACLE_PARSED_DEPENDENCY_H

#include <string>

#include "aur/package.hh"

namespace auracle {

// ParsedDependency provides a simple interface around dependency resolution.
class ParsedDependency final {
 public:
  // Constructs a ParsedDependency described by the given |depstring|. A
  // depstring follows the same format as that described by libalpm.
  explicit ParsedDependency(std::string_view depstring);

  ParsedDependency(ParsedDependency&&) = default;
  ParsedDependency& operator=(ParsedDependency&&) = default;

  ParsedDependency(const ParsedDependency&) = default;
  ParsedDependency& operator=(const ParsedDependency&) = default;

  const std::string& name() const { return name_; }

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
