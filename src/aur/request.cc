// SPDX-License-Identifier: MIT
#include "aur/request.hh"

#include <curl/curl.h>

#include <cstring>
#include <memory>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

namespace aur {

namespace {

std::string UrlEscape(const std::string_view sv) {
  char* ptr = curl_easy_escape(nullptr, sv.data(), sv.size());
  std::string escaped(ptr);
  curl_free(ptr);

  return escaped;
}

// Escapes a URL path, preserving '/' as a segment separator. This allows
// e.g. paths to files in subdirectories of a repo while still escaping
// characters that would otherwise be misinterpreted by curl or the server
// (spaces, '#', '?', etc).
std::string UrlEscapePath(std::string_view path) {
  return absl::StrJoin(absl::StrSplit(path, '/'), "/",
                       [](std::string* out, std::string_view segment) {
                         absl::StrAppend(out, UrlEscape(segment));
                       });
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

SearchRequest::SearchRequest(SearchBy by, std::string_view arg)
    : RpcRequest(HttpRequest::Command::GET,
                 absl::StrFormat("/rpc/v5/search/%s?by=%s", UrlEscape(arg),
                                 SearchByToString(by))) {}

SearchRequest::SearchBy SearchRequest::ParseSearchBy(
    std::string_view searchby) {
  if (searchby == "name") {
    return SearchBy::NAME;
  }
  if (searchby == "name-desc") {
    return SearchBy::NAME_DESC;
  }
  if (searchby == "maintainer") {
    return SearchBy::MAINTAINER;
  }
  if (searchby == "depends") {
    return SearchBy::DEPENDS;
  }
  if (searchby == "makedepends") {
    return SearchBy::MAKEDEPENDS;
  }
  if (searchby == "optdepends") {
    return SearchBy::OPTDEPENDS;
  }
  if (searchby == "checkdepends") {
    return SearchBy::CHECKDEPENDS;
  }
  if (searchby == "submitter") {
    return SearchBy::SUBMITTER;
  }
  if (searchby == "provides") {
    return SearchBy::PROVIDES;
  }
  if (searchby == "conflicts") {
    return SearchBy::CONFLICTS;
  }
  if (searchby == "replaces") {
    return SearchBy::REPLACES;
  }
  if (searchby == "keywords") {
    return SearchBy::KEYWORDS;
  }
  if (searchby == "groups") {
    return SearchBy::GROUPS;
  }
  if (searchby == "comaintainers") {
    return SearchBy::COMAINTAINERS;
  }
  return SearchBy::INVALID;
}

std::string SearchRequest::SearchByToString(SearchBy by) {
  switch (by) {
    case SearchBy::NAME:
      return "name";
    case SearchBy::NAME_DESC:
      return "name-desc";
    case SearchBy::MAINTAINER:
      return "maintainer";
    case SearchBy::DEPENDS:
      return "depends";
    case SearchBy::MAKEDEPENDS:
      return "makedepends";
    case SearchBy::OPTDEPENDS:
      return "optdepends";
    case SearchBy::CHECKDEPENDS:
      return "checkdepends";
    case SearchBy::SUBMITTER:
      return "submitter";
    case SearchBy::PROVIDES:
      return "provides";
    case SearchBy::CONFLICTS:
      return "conflicts";
    case SearchBy::REPLACES:
      return "replaces";
    case SearchBy::KEYWORDS:
      return "keywords";
    case SearchBy::GROUPS:
      return "groups";
    case SearchBy::COMAINTAINERS:
      return "comaintainers";
    default:
      return "";
  }
}

// static
RawRequest RawRequest::ForSourceFile(const Package& package,
                                     std::string_view filename) {
  return RawRequest(absl::StrCat("/cgit/aur.git/plain/",
                                 UrlEscapePath(filename),
                                 "?h=", UrlEscape(package.pkgbase)));
}

std::string RawRequest::Url(std::string_view baseurl) const {
  return absl::StrCat(baseurl, urlpath_);
}

std::string CloneRequest::Url(std::string_view baseurl) const {
  return absl::StrCat(baseurl, "/", reponame_);
}

}  // namespace aur
