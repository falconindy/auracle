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
      this->aur_baseurl = std::move(aur_baseurl);
      return *this;
    }

    Options& set_pacman(Pacman* pacman) {
      this->pacman = pacman;
      return *this;
    }

    Options& set_quiet(bool quiet) {
      this->quiet = quiet;
      return *this;
    }

    std::string aur_baseurl;
    Pacman* pacman = nullptr;
    bool quiet = false;
  };

  explicit Auracle(Options options);

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
  int Info(const std::vector<std::string>& args, const CommandOptions& options);
  int Show(const std::vector<std::string>& args, const CommandOptions& options);
  int RawInfo(const std::vector<std::string>& args,
              const CommandOptions& options);
  int RawSearch(const std::vector<std::string>& args,
                const CommandOptions& options);
  int Search(const std::vector<std::string>& args,
             const CommandOptions& options);
  int Outdated(const std::vector<std::string>& args,
               const CommandOptions& options);

 private:
  struct PackageIterator {
    using PackageCallback = std::function<void(const aur::Package&)>;

    PackageIterator(bool recurse, PackageCallback callback)
        : recurse(recurse), callback(std::move(callback)) {}

    bool recurse;

    const PackageCallback callback;
    PackageCache package_cache;
  };

  void IteratePackages(std::vector<std::string> args, PackageIterator* state);

  std::unique_ptr<aur::Aur> aur_;
  Pacman* pacman_;
};

}  // namespace auracle

#endif  // AURACLE_HH
