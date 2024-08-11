// SPDX-License-Identifier: MIT
#ifndef AURACLE_FORMAT_HH_
#define AURACLE_FORMAT_HH_

#include <string_view>

#include "absl/status/status.h"
#include "aur/package.hh"
#include "auracle/pacman.hh"

namespace format {

void NameOnly(const aur::Package& package);
void Update(const auracle::Pacman::Package& from, const aur::Package& to);
void Short(const aur::Package& package,
           const std::optional<auracle::Pacman::Package>& local_package);
void Long(const aur::Package& package,
          const std::optional<auracle::Pacman::Package>& local_package);
void Custom(std::string_view format, const aur::Package& package);

absl::Status Validate(std::string_view format);

}  // namespace format

#endif  // AURACLE_FORMAT_HH_
