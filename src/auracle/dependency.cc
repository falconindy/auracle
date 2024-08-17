#include "auracle/dependency.hh"

#include <alpm.h>

#include "absl/algorithm/container.h"

namespace auracle {

namespace {

int Vercmp(const std::string& a, const std::string& b) {
  return alpm_pkg_vercmp(a.c_str(), b.c_str());
}

}  // namespace

Dependency::Dependency(std::string_view depstring) : depstring_(depstring) {
  name_ = depstring;
  if (auto pos = depstring.find("<="); pos != depstring.npos) {
    mod_ = Mod::LE;
    name_ = depstring.substr(0, pos);
    version_ = depstring.substr(pos + 2);
  } else if (auto pos = depstring.find(">="); pos != depstring.npos) {
    mod_ = Mod::GE;
    name_ = depstring.substr(0, pos);
    version_ = depstring.substr(pos + 2);
  } else if (auto pos = depstring.find_first_of("<>="); pos != depstring.npos) {
    switch (depstring[pos]) {
      case '<':
        mod_ = Mod::LT;
        break;
      case '>':
        mod_ = Mod::GT;
        break;
      case '=':
        mod_ = Mod::EQ;
        break;
    }

    name_ = depstring.substr(0, pos);
    version_ = depstring.substr(pos + 1);
  } else {
    name_ = depstring;
  }
}

bool Dependency::SatisfiedByVersion(const std::string& version) const {
  const int vercmp = Vercmp(version, version_);
  switch (mod_) {
    case Mod::EQ:
      return vercmp == 0;
    case Mod::GE:
      return vercmp >= 0;
    case Mod::GT:
      return vercmp > 0;
    case Mod::LE:
      return vercmp <= 0;
    case Mod::LT:
      return vercmp < 0;
    default:
      break;
  }

  return false;
}

bool Dependency::SatisfiedBy(const aur::Package& candidate) const {
  if (version_.empty()) {
    // exact match on package name
    if (name_ == candidate.name) {
      return true;
    }

    // Satisfied via provides without version comparison.
    for (const auto& depstring : candidate.provides) {
      if (name_ == Dependency(depstring).name_) {
        return true;
      }
    }
  } else {  // !version_.empty()
    // Exact match on package name and satisfied version
    if (name_ == candidate.name && SatisfiedByVersion(candidate.version)) {
      return true;
    }

    // Satisfied via provides with version comparison.
    for (const auto& depstring : candidate.provides) {
      Dependency provide(depstring);

      // An unversioned or malformed provide can't satisfy a versioned
      // dependency.
      if (provide.mod_ != Mod::EQ) {
        continue;
      }

      // Names must match.
      if (name_ != provide.name_) {
        continue;
      }

      // Compare versions.
      if (SatisfiedByVersion(provide.version_)) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace auracle
