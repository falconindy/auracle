// SPDX-License-Identifier: MIT
#include "request.hh"

#include <curl/curl.h>

#include <cstring>
#include <memory>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace aur {

namespace {

std::string UrlEscape(const std::string_view sv) {
  char* ptr = curl_easy_escape(nullptr, sv.data(), sv.size());
  std::string escaped(ptr);
  curl_free(ptr);

  return escaped;
}

void QueryParamFormatter(std::string* out, const HttpRequest::QueryParam& kv) {
  absl::StrAppend(out, kv.first, "=", UrlEscape(kv.second));
}

}  // namespace

void RpcRequest::AddArg(std::string_view key, std::string_view value) {
  args_.emplace_back(key, value);
}

std::vector<std::string> RpcRequest::Build(std::string_view baseurl) const {
  const auto next_span = [&](std::string_view s) -> std::string_view {
    // No chopping needed.
    if (s.size() <= approx_max_length_) {
      return s;
    }

    // Try to chop at the final ampersand before the cutoff length.
    auto n = s.substr(0, approx_max_length_).rfind('&');
    if (n != std::string_view::npos) {
      return s.substr(0, n);
    }

    // We found a single arg which is Too Damn Long. Look for the ampersand just
    // after the length limit and use all of it.
    n = s.substr(approx_max_length_).find('&');
    if (n != std::string_view::npos) {
      return s.substr(0, n + approx_max_length_);
    }

    // We're at the end of the querystring and have no place to chop.
    return s;
  };

  const auto qs = absl::StrJoin(args_, "&", &QueryParamFormatter);
  std::string_view sv(qs);

  std::vector<std::string> requests;
  while (!sv.empty()) {
    const auto span = next_span(sv);

    requests.push_back(
        absl::StrCat(baseurl, "/rpc?", base_querystring_, "&", span));
    sv.remove_prefix(std::min(sv.length(), span.length() + 1));
  }

  return requests;
}

RpcRequest::RpcRequest(const HttpRequest::QueryParams& base_params,
                       size_type approx_max_length)
    : base_querystring_(absl::StrJoin(base_params, "&", &QueryParamFormatter)),
      approx_max_length_(approx_max_length) {}

// static
RawRequest RawRequest::ForSourceFile(const Package& package,
                                     std::string_view filename) {
  return RawRequest(absl::StrCat("/cgit/aur.git/plain/", filename,
                                 "?h=", UrlEscape(package.pkgbase)));
}

std::vector<std::string> RawRequest::Build(std::string_view baseurl) const {
  return {absl::StrCat(baseurl, urlpath_)};
}

std::vector<std::string> CloneRequest::Build(std::string_view baseurl) const {
  return {absl::StrCat(baseurl, "/", reponame_)};
}

}  // namespace aur
