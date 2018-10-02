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

  friend std::ostream& operator<<(std::ostream& os, const NameOnly& f);

 private:
  const aur::Package& package;
};

struct Short {
  Short(const aur::Package& package) : package(package) {}
  ~Short() = default;

  friend std::ostream& operator<<(std::ostream& os, const Short& f);

 private:
  const aur::Package& package;
};

struct Long {
  Long(const aur::Package& package, const dlr::Pacman::Package* local_package)
      : package(package), local_package(local_package) {}
  ~Long() = default;

  friend std::ostream& operator<<(std::ostream& os, const Long& f);

 private:
  const aur::Package& package;
  const dlr::Pacman::Package* local_package;
};

struct Update {
  Update(const dlr::Pacman::Package& from, const aur::Package& to, bool ignored)
      : from(from), to(to), ignored(ignored) {}

  friend std::ostream& operator<<(std::ostream& os, const Update& u);

 private:
  const dlr::Pacman::Package& from;
  const aur::Package& to;
  const bool ignored;
};

// TODO: custom formatting
// Instead of printf crap, use something closer to python formatting, e.g.
//
// aur/{pkgname} ({votes:%d})\n    {description}
//
// look into Ragel for format compilation.
struct Custom {
  struct CompiledFormat {
    using FieldRef = std::function<std::string(const aur::Package&)>;
    using Token = std::variant<std::string, FieldRef>;

    std::vector<Token> tokens;
  };

  static CompiledFormat CompileFormat(const std::string& format);

  Custom(const aur::Package& package, const CompiledFormat& format);
  ~Custom() = default;

 private:
  const aur::Package& package;
  const CompiledFormat& format;
};

}  // namespace format

#endif  // FORMAT_HH
