#ifndef AURACLE_HH
#define AURACLE_HH

#include <string>
#include <vector>

#include "aur/aur.hh"
#include "package_cache.hh"
#include "pacman.hh"
#include "sort.hh"

namespace auracle {

class Auracle {
 public:
  struct Options {
    Options& set_aur_baseurl(std::string aur_baseurl) {
      this->aur_baseurl = aur_baseurl;
      return *this;
    }

    Options& set_pacman(Pacman* pacman) {
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
    Pacman* pacman = nullptr;
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

  Auracle(Auracle&&) = default;
  Auracle& operator=(Auracle&&) = default;

  struct CommandOptions {
    aur::SearchRequest::SearchBy search_by =
        aur::SearchRequest::SearchBy::NAME_DESC;
    std::string directory;
    bool recurse = false;
    bool allow_regex = true;
    bool quiet = false;
    std::string show_file = "PKGBUILD";
    sort::Sorter sorter =
        sort::MakePackageSorter("name", sort::OrderBy::ORDER_ASC);
    std::string format;
  };

  int BuildOrder(const std::vector<std::string>& args,
                 const CommandOptions& options);
  int Clone(const std::vector<std::string>& args,
            const CommandOptions& options);
  int Download(const std::vector<std::string>& args,
               const CommandOptions& options);
  int Info(const std::vector<std::string>& args, const CommandOptions& options);
  int Show(const std::vector<std::string>& args, const CommandOptions& options);
  int RawInfo(const std::vector<std::string>& args,
              const CommandOptions& options);
  int RawSearch(const std::vector<std::string>& args,
                const CommandOptions& options);
  int Search(const std::vector<std::string>& args,
             const CommandOptions& options);
  int Sync(const std::vector<std::string>& args, const CommandOptions& options);

 private:
  struct PackageIterator {
    using PackageCallback = std::function<void(const aur::Package&)>;

    PackageIterator(bool recurse, PackageCallback callback)
        : recurse(recurse), callback(std::move(callback)) {}

    bool recurse;

    const PackageCallback callback;
    PackageCache package_cache;
  };

  int SendRawRpc(const aur::RpcRequest* request);

  void IteratePackages(std::vector<std::string> args, PackageIterator* state);

  aur::Aur aur_;
  Pacman* const pacman_;
};

}  // namespace auracle

#endif  // AURACLE_HH
