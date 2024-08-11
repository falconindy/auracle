// SPDX-License-Identifier: MIT
#include "auracle/pacman.hh"

#include <glob.h>

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/types/span.h"

namespace {

class Glob {
 public:
  explicit Glob(std::string_view pattern) {
    glob_ok_ =
        glob(std::string(pattern).c_str(), GLOB_NOCHECK, nullptr, &glob_) == 0;
    if (glob_ok_) {
      results_ = absl::MakeSpan(glob_.gl_pathv, glob_.gl_pathc);
    }
  }

  bool ok() const { return glob_ok_; }

  ~Glob() {
    if (glob_ok_) {
      globfree(&glob_);
    }
  }

  using iterator = absl::Span<char*>::iterator;
  iterator begin() { return results_.begin(); }
  iterator end() { return results_.end(); }

 private:
  glob_t glob_;
  bool glob_ok_;
  absl::Span<char*> results_;
};

class FileReader {
 public:
  explicit FileReader(const std::string& path) : file_(path) {}

  ~FileReader() {
    if (ok()) {
      file_.close();
    }
  }

  bool GetLine(std::string& line) { return bool(std::getline(file_, line)); }

  bool ok() const { return file_.is_open(); }

 private:
  std::ifstream file_;
};

bool IsSection(std::string_view s) {
  return s.size() > 2 && s.front() == '[' && s.back() == ']';
}

std::pair<std::string_view, std::string_view> SplitKeyValue(
    std::string_view line) {
  auto equals = line.find('=');
  if (equals == line.npos) {
    return {line, ""};
  }

  return {absl::StripTrailingAsciiWhitespace(line.substr(0, equals)),
          absl::StripLeadingAsciiWhitespace(line.substr(equals + 1))};
}

}  // namespace

namespace auracle {

Pacman::Pacman(alpm_handle_t* alpm)
    : alpm_(alpm), local_db_(alpm_get_localdb(alpm_)) {}

Pacman::~Pacman() { alpm_release(alpm_); }

struct ParseState {
  std::string dbpath = "/var/lib/pacman";
  std::string rootdir = "/";

  std::string section;
  std::vector<std::string> repos;
};

bool ParseOneFile(const std::string& path, ParseState* state) {
  FileReader reader(path);

  for (std::string buffer; reader.GetLine(buffer);) {
    std::string_view line = absl::StripAsciiWhitespace(buffer);
    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (IsSection(line)) {
      state->section = line.substr(1, line.size() - 2);
      continue;
    }

    auto [key, value] = SplitKeyValue(line);
    if (value.empty()) {
      // There aren't any directives we care about which are valueless.
      continue;
    }

    if (state->section == "options") {
      if (key == "DBPath") {
        state->dbpath = value;
      } else if (key == "RootDir") {
        state->rootdir = value;
      }
    } else {
      state->repos.emplace_back(state->section);
    }

    if (key == "Include") {
      Glob includes(value);
      if (!includes.ok()) {
        return false;
      }

      for (const auto* p : includes) {
        if (!ParseOneFile(p, state)) {
          return false;
        }
      }
    }
  }

  return reader.ok();
}

// static
std::unique_ptr<Pacman> Pacman::NewFromConfig(const std::string& config_file) {
  ParseState state;

  if (!ParseOneFile(config_file, &state)) {
    return nullptr;
  }

  alpm_errno_t err;
  alpm_handle_t* alpm = alpm_initialize("/", state.dbpath.c_str(), &err);
  if (alpm == nullptr) {
    return nullptr;
  }

  for (const auto& repo : state.repos) {
    alpm_register_syncdb(alpm, repo.c_str(), alpm_siglevel_t(0));
  }

  return std::unique_ptr<Pacman>(new Pacman(alpm));
}

std::string Pacman::RepoForPackage(const std::string& package) const {
  for (auto i = alpm_get_syncdbs(alpm_); i != nullptr; i = i->next) {
    auto db = static_cast<alpm_db_t*>(i->data);
    auto pkgcache = alpm_db_get_pkgcache(db);

    if (alpm_find_satisfier(pkgcache, package.c_str()) != nullptr) {
      return alpm_db_get_name(db);
    }
  }

  return std::string();
}

bool Pacman::DependencyIsSatisfied(const std::string& package) const {
  auto* cache = alpm_db_get_pkgcache(local_db_);
  return alpm_find_satisfier(cache, package.c_str()) != nullptr;
}

std::optional<Pacman::Package> Pacman::GetLocalPackage(
    const std::string& name) const {
  auto* pkg = alpm_db_get_pkg(local_db_, name.c_str());
  if (pkg == nullptr) {
    return std::nullopt;
  }

  return Package{alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg)};
}

std::vector<Pacman::Package> Pacman::LocalPackages() const {
  std::vector<Package> packages;

  for (auto i = alpm_db_get_pkgcache(local_db_); i != nullptr; i = i->next) {
    const auto pkg = static_cast<alpm_pkg_t*>(i->data);

    packages.emplace_back(alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
  }

  return packages;
}

// static
int Pacman::Vercmp(const std::string& a, const std::string& b) {
  return alpm_pkg_vercmp(a.c_str(), b.c_str());
}

}  // namespace auracle
