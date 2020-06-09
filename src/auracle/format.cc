#include "format.hh"

#include <fmt/printf.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

#include "absl/time/time.h"
#include "terminal.hh"

namespace {

template <typename T>
struct Field {
  Field(std::string_view name, const T& value) : name(name), value(value) {}

  const std::string_view name;
  const T& value;
};

struct OptDepends {
  const std::vector<std::string>& optdepends;

  bool empty() const { return optdepends.empty(); }
};

template <typename T>
struct is_containerlike {
 private:
  template <typename U>
  static decltype((void)std::declval<U>().empty(), void(), std::true_type())
  test(int);

  template <typename>
  static std::false_type test(...);

 public:
  enum { value = decltype(test<T>(0))::value };
};

fmt::format_parse_context::iterator parse_format_or_default(
    fmt::format_parse_context& ctx, std::string_view default_value,
    std::string* out) {
  auto iter = std::find(ctx.begin(), ctx.end(), '}');
  if (ctx.begin() == iter) {
    *out = default_value;
  } else {
    *out = {ctx.begin(), iter};
  }

  return iter;
}

}  // namespace

FMT_BEGIN_NAMESPACE

template <>
struct formatter<absl::Time> {
  auto parse(fmt::format_parse_context& ctx) {
    return parse_format_or_default(ctx, "%c", &tm_format);
  }

  auto format(const absl::Time t, fmt::format_context& ctx) {
    return format_to(ctx.out(), "{}",
                     absl::FormatTime(tm_format, t, absl::LocalTimeZone()));
  }

  std::string tm_format;
};

// Specialization for formatting vectors of things with an optional custom
// delimiter.
template <typename T>
struct formatter<std::vector<T>> {
  auto parse(fmt::format_parse_context& ctx) {
    return parse_format_or_default(ctx, "  ", &delimiter);
  }

  auto format(const std::vector<T>& vec, fmt::format_context& ctx) {
    const char* sep = "";
    for (const auto& v : vec) {
      format_to(ctx.out(), "{}{}", sep, v);
      sep = &delimiter[0];
    }

    return ctx.out();
  }

 private:
  std::string delimiter;
};

// Specialization to format Dependency objects
template <>
struct formatter<aur::Dependency> : formatter<std::string_view> {
  auto format(const aur::Dependency& dep, fmt::format_context& ctx) {
    return formatter<std::string_view>::format(dep.depstring, ctx);
  }
};

// Specialization to format optdepends since we write these newline delimited,
// not double-space delimited.
template <>
struct formatter<OptDepends> : formatter<std::vector<std::string>> {
  auto format(const OptDepends& optdep, fmt::format_context& ctx) {
    return formatter<std::vector<std::string>>::format(optdep.optdepends, ctx);
  }

 private:
  std::string delimiter = "\n                 ";
};

template <typename T>
struct formatter<Field<T>> : formatter<std::string_view> {
  auto format(const Field<T>& f, fmt::format_context& ctx) {
    if constexpr (is_containerlike<T>::value) {
      if (f.value.empty()) {
        return ctx.out();
      }
    }

    return format_to(ctx.out(), "{:14s} : {}\n", f.name, f.value);
  }
};

FMT_END_NAMESPACE

namespace format {

void NameOnly(const aur::Package& package) {
  std::cout << fmt::format(terminal::Bold("{}\n"), package.name);
}

void Short(const aur::Package& package,
           const std::optional<auracle::Pacman::Package>& local_package) {
  namespace t = terminal;

  const auto& l = local_package;
  const auto& p = package;
  const auto ood_color =
      p.out_of_date > absl::UnixEpoch() ? &t::BoldRed : &t::BoldGreen;

  std::string installed_package;
  if (l) {
    const auto local_ver_color =
        auracle::Pacman::Vercmp(l->pkgver, p.version) < 0 ? &t::BoldRed
                                                          : &t::BoldGreen;
    installed_package =
        fmt::format("[installed: {}]", local_ver_color(l->pkgver));
  }

  std::cout << fmt::format("{}{} {} ({}, {}) {}\n    {}\n",
                           t::BoldMagenta("aur/"), t::Bold(p.name),
                           ood_color(p.version), p.votes, p.popularity,
                           installed_package, p.description);
}

void Long(const aur::Package& package,
          const std::optional<auracle::Pacman::Package>& local_package) {
  namespace t = terminal;

  const auto& l = local_package;
  const auto& p = package;

  const auto ood_color =
      p.out_of_date > absl::UnixEpoch() ? &t::BoldRed : &t::BoldGreen;

  std::string installed_package;
  if (l) {
    const auto local_ver_color =
        auracle::Pacman::Vercmp(l->pkgver, p.version) < 0 ? &t::BoldRed
                                                          : &t::BoldGreen;
    installed_package =
        fmt::format(" [installed: {}]", local_ver_color(l->pkgver));
  }

  std::cout << fmt::format("{}", Field("Repository", t::BoldMagenta("aur")));
  std::cout << fmt::format("{}", Field("Name", p.name));
  std::cout << fmt::format(
      "{}", Field("Version", ood_color(p.version) + installed_package));

  if (p.name != p.pkgbase) {
    std::cout << fmt::format("{}", Field("PackageBase", p.pkgbase));
  }

  std::cout << fmt::format("{}", Field("URL", t::BoldCyan(p.upstream_url)));
  std::cout << fmt::format(
      "{}", Field("AUR Page",
                  t::BoldCyan("https://aur.archlinux.org/packages/" + p.name)));
  std::cout << fmt::format("{}", Field("Keywords", p.keywords));
  std::cout << fmt::format("{}", Field("Groups", p.groups));
  std::cout << fmt::format("{}", Field("Depends On", p.depends));
  std::cout << fmt::format("{}", Field("Makedepends", p.makedepends));
  std::cout << fmt::format("{}", Field("Checkdepends", p.checkdepends));
  std::cout << fmt::format("{}", Field("Provides", p.provides));
  std::cout << fmt::format("{}", Field("Conflicts With", p.conflicts));
  std::cout << fmt::format("{}",
                           Field("Optional Deps", OptDepends{p.optdepends}));
  std::cout << fmt::format("{}", Field("Replaces", p.replaces));
  std::cout << fmt::format("{}", Field("Licenses", p.licenses));
  std::cout << fmt::format("{}", Field("Votes", p.votes));
  std::cout << fmt::format("{}", Field("Popularity", p.popularity));
  std::cout << fmt::format(
      "{}",
      Field("Maintainer", p.maintainer.empty() ? "(orphan)" : p.maintainer));
  std::cout << fmt::format("{}", Field("Submitted", p.submitted));
  std::cout << fmt::format("{}", Field("Last Modified", p.modified));
  if (p.out_of_date > absl::UnixEpoch()) {
    std::cout << fmt::format("{}", Field("Out of Date", p.out_of_date));
  }
  std::cout << fmt::format("{}", Field("Description", p.description));
  std::cout << fmt::format("\n");
}

void Update(const auracle::Pacman::Package& from, const aur::Package& to) {
  namespace t = terminal;

  std::cout << fmt::format("{} {} -> {}\n", t::Bold(from.pkgname),
                           t::BoldRed(from.pkgver), t::BoldGreen(to.version));
}

namespace {

void FormatCustomTo(std::string& out, const std::string& format,
                    const aur::Package& package) {
  // clang-format off
  fmt::format_to(
      std::back_inserter(out), format,
      fmt::arg("name", package.name),
      fmt::arg("description", package.description),
      fmt::arg("maintainer", package.maintainer),
      fmt::arg("version", package.version),
      fmt::arg("pkgbase", package.pkgbase),
      fmt::arg("url", package.upstream_url),

      fmt::arg("votes", package.votes),
      fmt::arg("popularity", package.popularity),

      fmt::arg("submitted", package.submitted),
      fmt::arg("modified", package.modified),
      fmt::arg("outofdate", package.out_of_date),

      fmt::arg("depends", package.depends),
      fmt::arg("makedepends", package.makedepends),
      fmt::arg("checkdepends", package.checkdepends),

      fmt::arg("conflicts", package.conflicts),
      fmt::arg("groups", package.groups),
      fmt::arg("keywords", package.keywords),
      fmt::arg("licenses", package.licenses),
      fmt::arg("optdepends", package.optdepends),
      fmt::arg("provides", package.provides),
      fmt::arg("replaces", package.replaces));
  // clang-format on
}

}  // namespace

void Custom(const std::string& format, const aur::Package& package) {
  std::string out;

  FormatCustomTo(out, format, package);

  std::cout << out << "\n";
}

bool FormatIsValid(const std::string& format, std::string* error) {
  try {
    std::string out;
    FormatCustomTo(out, format, aur::Package());
  } catch (const fmt::format_error& e) {
    error->assign(e.what());
    return false;
  }

  return true;
}

}  // namespace format
