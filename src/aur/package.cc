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

void from_json(const nlohmann::json& j, std::chrono::seconds& s) {
  if (j.is_null()) {
    return;
  }

  s = std::chrono::seconds(j);
}

void from_json(const nlohmann::json& j, Package& p) {
  for (auto iter : j.items()) {
    const auto& key = iter.key();
    const auto& value = iter.value();

    if (key == "Name") {
      from_json(value, p.name);
    } else if (key == "Description") {
      from_json(value, p.description);
    } else if (key == "Maintainer") {
      from_json(value, p.maintainer);
    } else if (key == "PackageBase") {
      from_json(value, p.pkgbase);
    } else if (key == "URL") {
      from_json(value, p.upstream_url);
    } else if (key == "URLPath") {
      from_json(value, p.aur_urlpath);
    } else if (key == "Version") {
      from_json(value, p.version);
    } else if (key == "Conflicts") {
      from_json(value, p.conflicts);
    } else if (key == "Groups") {
      from_json(value, p.groups);
    } else if (key == "Keywords") {
      from_json(value, p.keywords);
    } else if (key == "License") {
      from_json(value, p.licenses);
    } else if (key == "OptDepends") {
      from_json(value, p.optdepends);
    } else if (key == "Provides") {
      from_json(value, p.provides);
    } else if (key == "Replaces") {
      from_json(value, p.replaces);
    } else if (key == "CheckDepends") {
      from_json(value, p.checkdepends);
    } else if (key == "Depends") {
      from_json(value, p.depends);
    } else if (key == "MakeDepends") {
      from_json(value, p.makedepends);
    } else if (key == "ID") {
      from_json(value, p.package_id);
    } else if (key == "PackageBaseID") {
      from_json(value, p.pkgbase_id);
    } else if (key == "NumVotes") {
      from_json(value, p.votes);
    } else if (key == "Popularity") {
      from_json(value, p.popularity);
    } else if (key == "FirstSubmitted") {
      from_json(value, p.submitted_s);
    } else if (key == "LastModified") {
      from_json(value, p.modified_s);
    } else if (key == "OutOfDate") {
      from_json(value, p.out_of_date);
    }
  }
}

}  // namespace aur
