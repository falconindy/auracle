// SPDX-License-Identifier: MIT
#include "auracle/format.hh"

#include <iomanip>
#include <iostream>
#include <string_view>

#include "absl/time/time.h"
#include "auracle/terminal.hh"
#include "fmt/printf.h"

namespace {

template <typename T>
struct Field {
  constexpr Field(std::string_view name, const T& value)
      : name(name), value(value) {}

  const std::string_view name;
  const T& value;
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
    return parse_format_or_default(ctx, absl::RFC3339_sec, &tm_format);
  }

  auto format(const absl::Time t, fmt::format_context& ctx) {
    return fmt::format_to(
        ctx.out(), "{}", absl::FormatTime(tm_format, t, absl::LocalTimeZone()));
  }

  std::string tm_format;
};

// Specialization for formatting vectors of things with an optional custom
// delimiter.
template <typename T>
struct formatter<std::vector<T>> {
  auto parse(fmt::format_parse_context& ctx) {
    return parse_format_or_default(ctx, "  ", &delimiter_);
  }

  auto format(const std::vector<T>& vec, fmt::format_context& ctx) {
    std::string_view sep;
    for (const auto& v : vec) {
      fmt::format_to(ctx.out(), "{}{}", sep, v);
      sep = delimiter_;
    }

    return ctx.out();
  }

 private:
  std::string delimiter_;
};

template <typename T>
struct formatter<Field<T>> : formatter<std::string_view> {
  auto format(const Field<T>& f, fmt::format_context& ctx) {
    if constexpr (is_containerlike<T>::value) {
      if (f.value.empty()) {
        return ctx.out();
      }
    }

    // The value is not guaranteed to be consteval in this context, so the
    // format string has to be runtime evaluated. Icky, but at least this
    // is fully under our control.
    return fmt::format_to(ctx.out(), fmt::runtime("{:14s} : {}\n"), f.name,
                          f.value);
  }
};

FMT_END_NAMESPACE

namespace format {

void NameOnly(const aur::Package& package) {
  fmt::print("{}\n", terminal::Bold(package.name));
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

  fmt::print("{}{} {} ({}, {}) {}\n    {}\n", t::BoldMagenta("aur/"),
             t::Bold(p.name), ood_color(p.version), p.votes, p.popularity,
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

  fmt::print("{}", Field("Repository", t::BoldMagenta("aur")));
  fmt::print("{}", Field("Name", p.name));
  fmt::print("{}", Field("Version", ood_color(p.version) + installed_package));

  if (p.name != p.pkgbase) {
    fmt::print("{}", Field("PackageBase", p.pkgbase));
  }

  fmt::print("{}", Field("URL", t::BoldCyan(p.upstream_url)));
  fmt::print(
      "{}", Field("AUR Page",
                  t::BoldCyan("https://aur.archlinux.org/packages/" + p.name)));
  fmt::print("{}", Field("Keywords", p.keywords));
  fmt::print("{}", Field("Groups", p.groups));
  fmt::print("{}", Field("Depends On", p.depends));
  fmt::print("{}", Field("Makedepends", p.makedepends));
  fmt::print("{}", Field("Checkdepends", p.checkdepends));
  fmt::print("{}", Field("Provides", p.provides));
  fmt::print("{}", Field("Conflicts With", p.conflicts));
  fmt::print("{}", Field("Optional Deps", p.optdepends));
  fmt::print("{}", Field("Replaces", p.replaces));
  fmt::print("{}", Field("Licenses", p.licenses));
  fmt::print("{}", Field("Votes", p.votes));
  fmt::print("{}", Field("Popularity", p.popularity));
  fmt::print("{}", Field("Maintainer",
                         p.maintainer.empty() ? "(orphan)" : p.maintainer));
  fmt::print("{}", Field("Co-maintainers", p.comaintainers));
  fmt::print("{}", Field("Submitted", p.submitted));
  fmt::print("{}", Field("Last Modified", p.modified));
  if (p.out_of_date > absl::UnixEpoch()) {
    fmt::print("{}", Field("Out of Date", p.out_of_date));
  }
  fmt::print("{}", Field("Description", p.description));
  fmt::print("\n");
}

void Update(const auracle::Pacman::Package& from, const aur::Package& to) {
  namespace t = terminal;

  fmt::print("{} {} -> {}\n", t::Bold(from.pkgname), t::BoldRed(from.pkgver),
             t::BoldGreen(to.version));
}

namespace {

void FormatCustomTo(std::string& out, std::string_view format,
                    const aur::Package& package) {
  // clang-format off
  fmt::format_to(
      std::back_inserter(out), fmt::runtime(format),
      fmt::arg("name", package.name),
      fmt::arg("description", package.description),
      fmt::arg("maintainer", package.maintainer),
      fmt::arg("comaintainers", package.comaintainers),
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

void Custom(const std::string_view format, const aur::Package& package) {
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

absl::Status Validate(std::string_view format) {
  try {
    std::string out;
    FormatCustomTo(out, format, aur::Package());
  } catch (const fmt::format_error& e) {
    return absl::InvalidArgumentError(e.what());
  }
  return absl::OkStatus();
}

}  // namespace format
