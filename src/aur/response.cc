// SPDX-License-Identifier: MIT
#include "aur/response.hh"

#include "glaze/glaze.hpp"

template <>
struct glz::meta<aur::Package> {
  using T = aur::Package;
  static constexpr std::string_view name = "Package";

  template <std::string aur::Package::* F>
  static constexpr auto optional_string =
      custom<&T::read_optional<std::string, F>, F>;

  template <absl::Time aur::Package::* F>
  static constexpr auto absl_time = custom<&T::read_time<F>, &T::write_time<F>>;

  static constexpr auto value = object(                 //
      "CheckDepends", &T::checkdepends,                 //
      "CoMaintainers", &T::comaintainers,               //
      "Conflicts", &T::conflicts,                       //
      "Depends", &T::depends,                           //
      "Description", optional_string<&T::description>,  //
      "FirstSubmitted", absl_time<&T::submitted>,       //
      "Groups", &T::groups,                             //
      "ID", &T::package_id,                             //
      "Keywords", &T::keywords,                         //
      "LastModified", absl_time<&T::modified>,          //
      "License", &T::licenses,                          //
      "Maintainer", optional_string<&T::maintainer>,    //
      "MakeDepends", &T::makedepends,                   //
      "Name", &T::name,                                 //
      "NumVotes", &T::votes,                            //
      "OptDepends", &T::optdepends,                     //
      "OutOfDate", absl_time<&T::out_of_date>,          //
      "PackageBase", &T::pkgbase,                       //
      "PackageBaseID", &T::pkgbase_id,                  //
      "Popularity", &T::popularity,                     //
      "Provides", &T::provides,                         //
      "Replaces", &T::replaces,                         //
      "Submitter", &T::submitter,                       //
      "URL", optional_string<&T::upstream_url>,         //
      "URLPath", &T::aur_urlpath,                       //
      "Version", &T::version);
};

namespace aur {

struct Raw {
  std::vector<Package> packages;
  std::string error;

  struct glaze {
    static constexpr std::string_view name = "Raw";
    static constexpr auto value = glz::object(  //
        "results", &Raw::packages,              //
        "error", &Raw::error);
  };
};

absl::StatusOr<RpcResponse> RpcResponse::Parse(std::string_view bytes) {
  static constexpr glz::opts opts{
      .error_on_unknown_keys = false,
      .error_on_missing_keys = false,
  };

  Raw raw;
  const auto ec = glz::read<opts>(raw, bytes, glz::context{});
  if (ec) {
    return absl::InvalidArgumentError("parse error: " +
                                      glz::format_error(ec, bytes));
  } else if (!raw.error.empty()) {
    return absl::UnknownError(raw.error);
  }

  return RpcResponse(std::move(raw.packages));
}

}  // namespace aur
