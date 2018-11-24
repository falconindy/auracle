#ifndef PACMAN_HH
#define PACMAN_HH

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <alpm.h>

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

  // Returns true if the package is ignored.
  bool ShouldIgnorePackage(const std::string& package) const;

  // Returns the name of the repo that the package belongs to, or empty string
  // if the package was not found in any repo.
  std::string RepoForPackage(const std::string& package) const;

  bool HasPackage(const std::string& package) const {
    return !RepoForPackage(package).empty();
  }

  bool DependencyIsSatisfied(const std::string& package) const;

  // A list of installed packages which are not found in any currently enabled
  // Sync DB.
  std::vector<Package> ForeignPackages() const;

  std::optional<Package> GetLocalPackage(const std::string& name) const;

 private:
  Pacman(alpm_handle_t* alpm, std::vector<std::string> ignored_packages);

  alpm_handle_t* alpm_;
  alpm_db_t* local_db_;

  std::vector<std::string> ignored_packages_;
};

}  // namespace auracle

#endif  // PACMAN_HH
