// SPDX-License-Identifier: MIT
#include "package.hh"

#include "json_internal.hh"

namespace aur {

void from_json(const nlohmann::json& j, Dependency& d) {
  d.depstring = j;

  if (auto pos = d.depstring.find("<="); pos != std::string::npos) {
    d.mod = Dependency::Mod::LE;
    d.name = d.depstring.substr(0, pos);
    d.version = d.depstring.substr(pos + 2);
  } else if (auto pos = d.depstring.find(">="); pos != std::string::npos) {
    d.mod = Dependency::Mod::GE;
    d.name = d.depstring.substr(0, pos);
    d.version = d.depstring.substr(pos + 2);
  } else if (auto pos = d.depstring.find_first_of("<=>");
             pos != std::string::npos) {
    switch (d.depstring[pos]) {
      case '<':
        d.mod = Dependency::Mod::LT;
        break;
      case '>':
        d.mod = Dependency::Mod::GT;
        break;
      case '=':
        d.mod = Dependency::Mod::EQ;
        break;
    }

    d.name = d.depstring.substr(0, pos);
    d.version = d.depstring.substr(pos + 1);
  } else {
    d.name = d.depstring;
  }
}

void from_json(const nlohmann::json& j, absl::Time& t) {
  if (j.is_null()) {
    return;
  }

  t = absl::FromUnixSeconds(j);
}

void from_json(const nlohmann::json& j, Package& p) {
  // clang-format off
  static const auto& callbacks = *new CallbackMap<Package>{
    { "CheckDepends",     MakeValueCallback(&Package::checkdepends) },
    { "Conflicts",        MakeValueCallback(&Package::conflicts) },
    { "Depends",          MakeValueCallback(&Package::depends) },
    { "Description",      MakeValueCallback(&Package::description) },
    { "FirstSubmitted",   MakeValueCallback(&Package::submitted) },
    { "Groups",           MakeValueCallback(&Package::groups) },
    { "ID",               MakeValueCallback(&Package::package_id) },
    { "Keywords",         MakeValueCallback(&Package::keywords) },
    { "LastModified",     MakeValueCallback(&Package::modified) },
    { "License",          MakeValueCallback(&Package::licenses) },
    { "Maintainer",       MakeValueCallback(&Package::maintainer) },
    { "MakeDepends",      MakeValueCallback(&Package::makedepends) },
    { "Name",             MakeValueCallback(&Package::name) },
    { "NumVotes",         MakeValueCallback(&Package::votes) },
    { "OptDepends",       MakeValueCallback(&Package::optdepends) },
    { "OutOfDate",        MakeValueCallback(&Package::out_of_date) },
    { "PackageBase",      MakeValueCallback(&Package::pkgbase) },
    { "PackageBaseID",    MakeValueCallback(&Package::pkgbase_id) },
    { "Popularity",       MakeValueCallback(&Package::popularity) },
    { "Provides",         MakeValueCallback(&Package::provides) },
    { "Replaces",         MakeValueCallback(&Package::replaces) },
    { "URL",              MakeValueCallback(&Package::upstream_url) },
    { "URLPath",          MakeValueCallback(&Package::aur_urlpath) },
    { "Version",          MakeValueCallback(&Package::version) },
  };
  // clang-format on

  DeserializeJsonObject(j, callbacks, p);
}

}  // namespace aur
