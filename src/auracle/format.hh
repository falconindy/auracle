// SPDX-License-Identifier: MIT
#ifndef AURACLE_FORMAT_HH_
#define AURACLE_FORMAT_HH_

#include <string_view>

#include "aur/package.hh"
#include "pacman.hh"

namespace format {

void NameOnly(const aur::Package& package);
void Update(const auracle::Pacman::Package& from, const aur::Package& to);
void Short(const aur::Package& package,
           const std::optional<auracle::Pacman::Package>& local_package);
void Long(const aur::Package& package,
          const std::optional<auracle::Pacman::Package>& local_package);
void Custom(const std::string& format, const aur::Package& package);

bool FormatIsValid(const std::string& format, std::string* error);

}  // namespace format

#endif  // AURACLE_FORMAT_HH_
