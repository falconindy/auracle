#include "pacman.hh"

#include <fnmatch.h>
#include <glob.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace std {

template <>
struct default_delete<glob_t> {
  void operator()(glob_t* globbuf) {
    globfree(globbuf);
    delete globbuf;
  }
};

}  // namespace std

namespace {

std::string& ltrim(std::string& s) {
  s.erase(s.begin(),
          std::find_if(s.begin(), s.end(),
                       std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

std::string& rtrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       std::not1(std::ptr_fun<int, int>(std::isspace)))
              .base(),
          s.end());
  return s;
}

std::string& trim(std::string& s) { return ltrim(rtrim(s)); }

bool IsSection(const std::string& s) {
  return s.size() > 2 && s[0] == '[' && s[s.size() - 1] == ']';
}

}  // namespace

namespace auracle {

Pacman::Pacman(alpm_handle_t* alpm, std::vector<std::string> ignored_packages)
    : alpm_(alpm),
      local_db_(alpm_get_localdb(alpm_)),
      ignored_packages_(std::move(ignored_packages)) {}

Pacman::~Pacman() { alpm_release(alpm_); }

struct ParseState {
  alpm_handle_t* alpm;
  std::string dbpath = "/var/lib/pacman";
  std::string rootdir = "/";

  std::string section;
  std::vector<std::string> ignorepkgs;
  std::vector<std::string> repos;
};

bool ParseOneFile(const std::string& path, ParseState* state) {
  std::ifstream file(path);

  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);

    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (IsSection(line)) {
      state->section = line.substr(1, line.size() - 2);
      continue;
    }

    auto equals = line.find_first_of('=');
    if (equals == std::string::npos) {
      // There aren't any directives we care about which are valueless.
      continue;
    }

    auto key = line.substr(0, equals);
    key = trim(key);

    auto value = line.substr(equals + 1);
    value = trim(value);

    if (state->section == "options") {
      if (key == "IgnorePkg") {
        std::istringstream iss(value);
        std::copy(std::istream_iterator<std::string>(iss),
                  std::istream_iterator<std::string>(),
                  std::back_inserter(state->ignorepkgs));
      } else if (key == "DBPath") {
        state->dbpath = value;
      } else if (key == "RootDir") {
        state->rootdir = value;
      }
    } else {
      state->repos.emplace_back(state->section);
    }

    if (key == "Include") {
      auto globbuf = std::make_unique<glob_t>();

      if (glob(value.c_str(), GLOB_NOCHECK, nullptr, globbuf.get()) != 0) {
        return false;
      }

      for (size_t i = 0; i < globbuf->gl_pathc; ++i) {
        if (ParseOneFile(globbuf->gl_pathv[i], state) == false) {
          return false;
        }
      }
    }
  }

  file.close();

  return true;
}

// static
std::unique_ptr<Pacman> Pacman::NewFromConfig(const std::string& config_file) {
  ParseState state;

  if (!ParseOneFile(config_file, &state)) {
    return nullptr;
  }

  alpm_errno_t err;
  state.alpm = alpm_initialize("/", state.dbpath.c_str(), &err);
  if (state.alpm == nullptr) {
    return nullptr;
  }

  for (const auto& repo : state.repos) {
    alpm_register_syncdb(state.alpm, repo.c_str(),
                         static_cast<alpm_siglevel_t>(0));
  }

  return std::unique_ptr<Pacman>(
      new Pacman(state.alpm, std::move(state.ignorepkgs)));
}

bool Pacman::ShouldIgnorePackage(const std::string& package) const {
  return std::find_if(ignored_packages_.cbegin(), ignored_packages_.cend(),
                      [&package](const std::string& p) {
                        return fnmatch(p.c_str(), package.c_str(), 0) == 0;
                      }) != ignored_packages_.cend();
}

std::string Pacman::RepoForPackage(const std::string& package) const {
  for (auto i = alpm_get_syncdbs(alpm_); i != nullptr; i = i->next) {
    auto db = static_cast<alpm_db_t*>(i->data);

    if (alpm_find_satisfier(alpm_db_get_pkgcache(db), package.c_str())) {
      return alpm_db_get_name(db);
    }
  }

  return std::string();
}

bool Pacman::DependencyIsSatisfied(const std::string& package) const {
  auto* cache = alpm_db_get_pkgcache(local_db_);
  return alpm_find_satisfier(cache, package.c_str()) != nullptr;
}

std::variant<bool, Pacman::Package> Pacman::GetLocalPackage(
    const std::string& name) const {
  auto* pkg = alpm_db_get_pkg(local_db_, name.c_str());
  if (pkg == nullptr) {
    return false;
  }

  return Package{alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg)};
}

std::vector<Pacman::Package> Pacman::ForeignPackages() const {
  std::vector<Package> packages;

  for (auto i = alpm_db_get_pkgcache(local_db_); i; i = i->next) {
    const auto pkg = static_cast<alpm_pkg_t*>(i->data);
    std::string pkgname(alpm_pkg_get_name(pkg));

    if (RepoForPackage(pkgname).empty()) {
      packages.emplace_back(std::move(pkgname), alpm_pkg_get_version(pkg));
    }
  }

  return packages;
}

// static
int Pacman::Vercmp(const std::string& a, const std::string& b) {
  return alpm_pkg_vercmp(a.c_str(), b.c_str());
}

}  // namespace auracle
