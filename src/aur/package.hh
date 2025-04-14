// SPDX-License-Identifier: MIT
#ifndef AUR_PACKAGE_H
#define AUR_PACKAGE_H

#include <string>
#include <vector>

#include "absl/time/time.h"

namespace aur {

struct Package {
  Package() = default;

  std::string name;
  std::string description;
  std::string submitter;
  std::string maintainer;
  std::string pkgbase;
  std::string upstream_url;
  std::string aur_urlpath;
  std::string version;

  int package_id = 0;
  int pkgbase_id = 0;
  int votes = 0;
  double popularity = 0.f;

  absl::Time out_of_date;
  absl::Time submitted;
  absl::Time modified;

  std::vector<std::string> conflicts;
  std::vector<std::string> groups;
  std::vector<std::string> keywords;
  std::vector<std::string> licenses;
  std::vector<std::string> optdepends;
  std::vector<std::string> provides;
  std::vector<std::string> replaces;
  std::vector<std::string> comaintainers;

  std::vector<std::string> depends;
  std::vector<std::string> makedepends;
  std::vector<std::string> checkdepends;

  // glaze serialization helpers

  template <typename T, T aur::Package::* F>
  void read_optional(const std::optional<T>& v) {
    if (v) (*this).*F = *v;
  }

  template <absl::Time aur::Package::* F>
  void read_time(std::optional<int64_t> seconds) {
    (*this).*F = absl::FromUnixSeconds(seconds.value_or(0));
  }

  template <absl::Time aur::Package::* F>
  int64_t write_time() {
    return absl::ToUnixSeconds((*this).*F);
  }
};

inline bool operator==(const Package& a, const Package& b) {
  return a.package_id == b.package_id && a.pkgbase_id == b.pkgbase_id;
}

}  // namespace aur

#endif  // AUR_PACKAGE_H
