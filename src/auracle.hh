#ifndef AURACLE_HH
#define AURACLE_HH

#include <filesystem>
#include <string>
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

    Options& set_quiet(bool quiet) {
      this->quiet = quiet;
      return *this;
    }

    std::string aur_baseurl;
    dlr::Pacman* pacman;
    bool quiet = false;
    int max_connections = 0;
    int connection_timeout = 0;
  };

  explicit Auracle(Options options)
      : aur_(options.aur_baseurl), pacman_(options.pacman) {
    aur_.SetMaxConnections(options.max_connections);
    aur_.SetConnectTimeout(options.connection_timeout);
  }

  ~Auracle() = default;
  Auracle(const Auracle&) = delete;
  Auracle& operator=(const Auracle&) = delete;

  struct CommandOptions {
    aur::SearchRequest::SearchBy search_by =
        aur::SearchRequest::SearchBy::NAME_DESC;
    std::filesystem::path directory;
    bool recurse = false;
    bool allow_regex = true;
    bool quiet = false;
  };

  int BuildOrder(const std::vector<PackageOrDependency>& args,
                 const CommandOptions& options);
  int Clone(const std::vector<PackageOrDependency>& args,
            const CommandOptions& options);
  int Download(const std::vector<PackageOrDependency>& args,
               const CommandOptions& options);
  int Info(const std::vector<PackageOrDependency>& args,
           const CommandOptions& options);
  int Pkgbuild(const std::vector<PackageOrDependency>& args,
               const CommandOptions& options);
  int RawInfo(const std::vector<PackageOrDependency>& args,
              const CommandOptions& options);
  int RawSearch(const std::vector<PackageOrDependency>& args,
                const CommandOptions& options);
  int Search(const std::vector<PackageOrDependency>& args,
             const CommandOptions& options);
  int Sync(const std::vector<PackageOrDependency>& args,
           const CommandOptions& options);

 private:
  struct PackageIterator {
    using PackageCallback = std::function<void(const aur::Package&)>;

    PackageIterator(bool recurse, PackageCallback callback)
        : recurse(recurse), callback(std::move(callback)) {}

    bool recurse;

    const PackageCallback callback;
    dlr::InMemoryRepo package_repo;
  };

  int SendRawRpc(const aur::RpcRequest* request);

  void IteratePackages(std::vector<PackageOrDependency> args,
                       PackageIterator* state);

  aur::Aur aur_;
  dlr::Pacman* const pacman_;
};

#endif  // AURACLE_HH
