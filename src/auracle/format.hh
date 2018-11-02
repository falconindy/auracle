#ifndef FORMAT_HH
#define FORMAT_HH

#include <string_view>

#include "aur/package.hh"
#include "pacman.hh"

namespace format {

void NameOnly(const aur::Package& package);
void Update(const auracle::Pacman::Package& from, const aur::Package& to,
            bool ignored);
void Short(const aur::Package& package);
void Long(const aur::Package& package,
          const auracle::Pacman::Package* local_package);

}  // namespace format

#endif  // FORMAT_HH
