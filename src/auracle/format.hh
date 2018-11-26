#ifndef FORMAT_HH
#define FORMAT_HH

#include <functional>
#include <iostream>
#include <variant>

#include "aur/package.hh"
#include "pacman.hh"

namespace format {

struct NameOnly {
  NameOnly(const aur::Package& package) : package(package) {}
  ~NameOnly() = default;

  friend std::ostream& operator<<(std::ostream& os, const NameOnly& n);

 private:
  const aur::Package& package;
};

struct Short {
  Short(const aur::Package& package) : package(package) {}
  ~Short() = default;

  friend std::ostream& operator<<(std::ostream& os, const Short& s);

 private:
  const aur::Package& package;
};

struct Long {
  Long(const aur::Package& package,
       const auracle::Pacman::Package* local_package)
      : package(package), local_package(local_package) {}
  ~Long() = default;

  friend std::ostream& operator<<(std::ostream& os, const Long& l);

 private:
  const aur::Package& package;
  const auracle::Pacman::Package* local_package;
};

struct Update {
  Update(const auracle::Pacman::Package& from, const aur::Package& to,
         bool ignored, bool needs_update)
      : from(from), to(to), ignored(ignored), needs_update(needs_update) {}

  friend std::ostream& operator<<(std::ostream& os, const Update& u);

 private:
  const auracle::Pacman::Package& from;
  const aur::Package& to;
  const bool ignored;
  const bool needs_update;
};

}  // namespace format

#endif  // FORMAT_HH
