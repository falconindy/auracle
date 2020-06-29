// SPDX-License-Identifier: MIT
#ifndef AUR_REQUEST_HH_
#define AUR_REQUEST_HH_

#include <string>
#include <utility>
#include <vector>

#include "package.hh"

namespace aur {

// Abstract class describing a request for a resource.
class Request {
 public:
  virtual ~Request() = default;

  virtual std::vector<std::string> Build(std::string_view baseurl) const = 0;
};

class HttpRequest : public Request {
 public:
  using QueryParam = std::pair<std::string, std::string>;
  using QueryParams = std::vector<QueryParam>;
};

// A class describing a GET request for an arbitrary URL on the AUR.
class RawRequest : public HttpRequest {
 public:
  static RawRequest ForSourceFile(const Package& package,
                                  std::string_view filename);

  explicit RawRequest(std::string urlpath) : urlpath_(std::move(urlpath)) {}

  RawRequest(const RawRequest&) = delete;
  RawRequest& operator=(const RawRequest&) = delete;

  RawRequest(RawRequest&&) = default;
  RawRequest& operator=(RawRequest&&) = default;

  std::vector<std::string> Build(std::string_view baseurl) const override;

 private:
  std::string urlpath_;
};

// A class describing a url for a git repo hosted on the AUR.
class CloneRequest : public Request {
 public:
  explicit CloneRequest(std::string reponame)
      : reponame_(std::move(reponame)) {}

  CloneRequest(const CloneRequest&) = delete;
  CloneRequest& operator=(const CloneRequest&) = delete;

  CloneRequest(CloneRequest&&) = default;
  CloneRequest& operator=(CloneRequest&&) = default;

  const std::string& reponame() const { return reponame_; }

  std::vector<std::string> Build(std::string_view baseurl) const override;

 private:
  std::string reponame_;
};

// A base class describing a GET request to the RPC endpoint of the AUR.
class RpcRequest : public HttpRequest {
 public:
  using size_type = std::string_view::size_type;

  // Upper limit on aur.archlinux.org seems to be somewhere around 8k.
  static constexpr size_type kMaxUriLength = 8000;

  RpcRequest(const HttpRequest::QueryParams& base_params,
             size_type approx_max_length = kMaxUriLength);

  RpcRequest(const RpcRequest&) = delete;
  RpcRequest& operator=(const RpcRequest&) = delete;

  RpcRequest(RpcRequest&&) = default;
  RpcRequest& operator=(RpcRequest&&) = default;

  std::vector<std::string> Build(std::string_view baseurl) const override;

  void AddArg(std::string_view key, std::string_view value);

 private:
  std::string base_querystring_;
  size_type approx_max_length_;

  HttpRequest::QueryParams args_;
};

class InfoRequest : public RpcRequest {
 public:
  explicit InfoRequest(const std::vector<std::string>& args) : InfoRequest() {
    for (const auto& arg : args) {
      AddArg(arg);
    }
  }

  InfoRequest(const InfoRequest&) = delete;
  InfoRequest& operator=(const InfoRequest&) = delete;

  InfoRequest(InfoRequest&&) = default;
  InfoRequest& operator=(InfoRequest&&) = default;

  InfoRequest() : RpcRequest({{"v", "5"}, {"type", "info"}}) {}

  void AddArg(std::string_view arg) { RpcRequest::AddArg("arg[]", arg); }
};

class SearchRequest : public RpcRequest {
 public:
  enum class SearchBy {
    INVALID,
    NAME,
    NAME_DESC,
    MAINTAINER,
    DEPENDS,
    MAKEDEPENDS,
    OPTDEPENDS,
    CHECKDEPENDS,
  };

  static SearchBy ParseSearchBy(std::string_view searchby) {
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
    return SearchBy::INVALID;
  }

  SearchRequest(SearchBy by, std::string_view arg)
      : RpcRequest({
            {"v", "5"},
            {"type", "search"},
            {"by", SearchByToString(by)},
        }) {
    AddArg("arg", arg);
  }

  SearchRequest(const SearchRequest&) = delete;
  SearchRequest& operator=(const SearchRequest&) = delete;

 private:
  std::string SearchByToString(SearchBy by) {
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
      default:
        return "";
    }
  }
};

}  // namespace aur

#endif  // AUR_REQUEST_HH_
