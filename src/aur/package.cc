#include "package.hh"

#include <stdio.h>
#include <time.h>

#include "json_internal.hh"

namespace aur {
namespace fields {

#define DEFINE_STRING_FIELD(JsonField, StructField) \
  DEFINE_FIELD(JsonField, std::string, Package::StructField)
DEFINE_STRING_FIELD(Name, name);
DEFINE_STRING_FIELD(Description, description);
DEFINE_STRING_FIELD(Maintainer, maintainer);
DEFINE_STRING_FIELD(PackageBase, pkgbase);
DEFINE_STRING_FIELD(URL, upstream_url);
DEFINE_STRING_FIELD(URLPath, aur_urlpath);
DEFINE_STRING_FIELD(Version, version);
#undef DEFINE_STRING_FIELD

#define DEFINE_VECTOR_DEPENDENCY_FIELD(JsonField, StructField) \
  DEFINE_FIELD(JsonField, std::vector<Dependency>, Package::StructField)
DEFINE_VECTOR_DEPENDENCY_FIELD(CheckDepends, checkdepends);
DEFINE_VECTOR_DEPENDENCY_FIELD(Depends, depends);
DEFINE_VECTOR_DEPENDENCY_FIELD(MakeDepends, makedepends);

#define DEFINE_VECTOR_STRING_FIELD(JsonField, StructField) \
  DEFINE_FIELD(JsonField, std::vector<std::string>, Package::StructField)
DEFINE_VECTOR_STRING_FIELD(Conflicts, conflicts);
DEFINE_VECTOR_STRING_FIELD(Groups, groups);
DEFINE_VECTOR_STRING_FIELD(Keywords, keywords);
DEFINE_VECTOR_STRING_FIELD(Licenses, licenses);
DEFINE_VECTOR_STRING_FIELD(OptDepends, optdepends);
DEFINE_VECTOR_STRING_FIELD(Provides, provides);
DEFINE_VECTOR_STRING_FIELD(Replaces, replaces);
#undef DEFINE_VECTOR_STRING_FIELD

#define DEFINE_INT_FIELD(JsonField, StructField) \
  DEFINE_FIELD(JsonField, int, Package::StructField)
DEFINE_INT_FIELD(ID, package_id);
DEFINE_INT_FIELD(PackageBaseID, pkgbase_id);
DEFINE_INT_FIELD(NumVotes, votes);
#undef DEFINE_INT_FIELD

#define DEFINE_DOUBLE_FIELD(JsonField, StructField) \
  DEFINE_FIELD(JsonField, double, Package::StructField)
DEFINE_DOUBLE_FIELD(Popularity, popularity);
#undef DEFINE_DOUBLE_FIELD

#define DEFINE_TIME_FIELD(JsonField, StructField) \
  DEFINE_FIELD(JsonField, time_t, Package::StructField)
DEFINE_TIME_FIELD(FirstSubmitted, submitted_s);
DEFINE_TIME_FIELD(LastModified, modified_s);
DEFINE_TIME_FIELD(OutOfDate, out_of_date);
#undef DEFINE_TIME_FIELD

}  // namespace fields

void from_json(const nlohmann::json& j, Dependency& d) {
  d.depstring = j.get<std::string>();

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

void from_json(const nlohmann::json& j, Package& p) {
  // std::string
  Store<fields::Name>(j, p);
  Store<fields::Description>(j, p);
  Store<fields::Maintainer>(j, p);
  Store<fields::PackageBase>(j, p);
  Store<fields::URL>(j, p);
  Store<fields::URLPath>(j, p);
  Store<fields::Version>(j, p);

  // std::vector<std::string>
  Store<fields::Conflicts>(j, p);
  Store<fields::Groups>(j, p);
  Store<fields::Keywords>(j, p);
  Store<fields::Licenses>(j, p);
  Store<fields::OptDepends>(j, p);
  Store<fields::Provides>(j, p);
  Store<fields::Replaces>(j, p);

  // std::vector<Dependency>
  Store<fields::CheckDepends>(j, p);
  Store<fields::Depends>(j, p);
  Store<fields::MakeDepends>(j, p);

  // int
  Store<fields::ID>(j, p);
  Store<fields::PackageBaseID>(j, p);
  Store<fields::NumVotes>(j, p);

  // double
  Store<fields::Popularity>(j, p);

  // time_t
  Store<fields::FirstSubmitted>(j, p);
  Store<fields::LastModified>(j, p);
  Store<fields::OutOfDate>(j, p);
}

}  // namespace aur
