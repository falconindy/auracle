// SPDX-License-Identifier: MIT
#include "aur/request.hh"

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

std::string RpcRequest::Url(std::string_view baseurl) const {
  return absl::StrCat(baseurl, endpoint_);
}

void RpcRequest::AddArg(std::string key, std::string value) {
  params_.push_back({std::move(key), std::move(value)});
}

std::string RpcRequest::Payload() const {
  return absl::StrJoin(params_, "&", QueryParamFormatter);
}

// static
RawRequest RawRequest::ForSourceFile(const Package& package,
                                     std::string_view filename) {
  return RawRequest(absl::StrCat("/cgit/aur.git/plain/", filename,
                                 "?h=", UrlEscape(package.pkgbase)));
}

std::string RawRequest::Url(std::string_view baseurl) const {
  return absl::StrCat(baseurl, urlpath_);
}

std::string CloneRequest::Url(std::string_view baseurl) const {
  return absl::StrCat(baseurl, "/", reponame_);
}

}  // namespace aur
