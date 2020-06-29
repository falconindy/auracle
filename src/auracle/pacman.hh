// SPDX-License-Identifier: MIT
#ifndef AURACLE_PACMAN_HH_
#define AURACLE_PACMAN_HH_

#include <alpm.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace auracle {

class Pacman {
 public:
  struct Package {
    Package(std::string pkgname, std::string pkgver)
        : pkgname(std::move(pkgname)), pkgver(std::move(pkgver)) {}
    std::string pkgname;
    std::string pkgver;
  };

  // Factory constructor.
  static std::unique_ptr<Pacman> NewFromConfig(const std::string& config_file);

  ~Pacman();

  Pacman(const Pacman&) = delete;
  Pacman& operator=(const Pacman&) = delete;

  Pacman(Pacman&&) = default;
  Pacman& operator=(Pacman&&) = default;

  static int Vercmp(const std::string& a, const std::string& b);

  // Returns the name of the repo that the package belongs to, or empty string
  // if the package was not found in any repo.
  std::string RepoForPackage(const std::string& package) const;

  bool HasPackage(const std::string& package) const {
    return !RepoForPackage(package).empty();
  }

  bool DependencyIsSatisfied(const std::string& package) const;

  std::vector<Package> LocalPackages() const;
  std::optional<Package> GetLocalPackage(const std::string& name) const;

 private:
  Pacman(alpm_handle_t* alpm);

  alpm_handle_t* alpm_;
  alpm_db_t* local_db_;
};

}  // namespace auracle

#endif  // AURACLE_PACMAN_HH_
