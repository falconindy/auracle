// SPDX-License-Identifier: MIT
#include "aur/package.hh"

#include "absl/base/no_destructor.h"
#include "aur/json_internal.hh"

namespace absl {

void from_json(const nlohmann::json& j, Time& t) {
  t = absl::FromUnixSeconds(j);
}

}  // namespace absl

namespace aur {

void from_json(const nlohmann::json& j, Package& p) {
  // clang-format off
  static const absl::NoDestructor<CallbackMap<Package>> kCallbacks({
    { "CheckDepends",     MakeValueCallback(&Package::checkdepends) },
    { "CoMaintainers",    MakeValueCallback(&Package::comaintainers) },
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
  });
  // clang-format on

  DeserializeJsonObject(j, *kCallbacks, p);
}

}  // namespace aur
