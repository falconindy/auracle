#ifndef AURACLE_HH
#define AURACLE_HH

#include <iostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "aur/aur.hh"
#include "inmemory_repo.hh"
#include "pacman.hh"

class PackageOrDependency : public std::variant<std::string, aur::Dependency> {
 public:
  using std::variant<std::string, aur::Dependency>::variant;

  operator std::string() const {
    auto pval = std::get_if<std::string>(this);
    if (pval != nullptr) {
      return *pval;
    } else {
      return std::get_if<aur::Dependency>(this)->name;
    }
  }

  bool operator==(const std::string& other) const {
    return std::string(*this) == other;
  }
};

class Auracle {
 public:
  struct Options {
    Options& set_aur_baseurl(std::string aur_baseurl) {
      this->aur_baseurl = aur_baseurl;
      return *this;
    }

    Options& set_pacman(dlr::Pacman* pacman) {
      this->pacman = pacman;
      return *this;
    }

    Options& set_max_connections(int max_connections) {
      this->max_connections = max_connections;
      return *this;
    }

    Options& set_connection_timeout(int connection_timeout) {
      this->connection_timeout = connection_timeout;
      return *this;
    }

    Options& set_allow_regex(bool allow_regex) {
      this->allow_regex = allow_regex;
      return *this;
    }

    Options& set_quiet(bool quiet) {
      this->quiet = quiet;
      return *this;
    }

    std::string aur_baseurl;
    dlr::Pacman* pacman;
    bool allow_regex = true;
    bool quiet = false;
    int max_connections = 0;
    int connection_timeout = 0;
  };

  explicit Auracle(Options options)
      : options_(std::move(options)),
        aur_(options_.aur_baseurl),
        allow_regex_(options_.allow_regex),
        pacman_(options_.pacman) {
    aur_.SetMaxConnections(options_.max_connections);
    aur_.SetConnectTimeout(options_.connection_timeout);
  }

  ~Auracle() = default;
  Auracle(const Auracle&) = delete;
  Auracle& operator=(const Auracle&) = delete;

  int Info(const std::vector<PackageOrDependency>& args);
  int Search(const std::vector<PackageOrDependency>& args,
             aur::SearchRequest::SearchBy by);
  int Download(const std::vector<PackageOrDependency>& args, bool recurse);
  int Sync(const std::vector<PackageOrDependency>& args);
  int BuildOrder(const std::vector<PackageOrDependency>& args);
  int Pkgbuild(const std::vector<PackageOrDependency>& args);

 private:
  struct PackageIterator {
    explicit PackageIterator(bool recurse, bool download)
        : recurse(recurse), download(download) {}

    bool recurse;
    bool download;

    dlr::InMemoryRepo package_repo;
  };

  void IteratePackages(std::vector<PackageOrDependency> args,
                       PackageIterator* state);

  const Options options_;
  aur::Aur aur_;
  bool allow_regex_;
  dlr::Pacman* const pacman_;
};

#endif  // AURACLE_HH
