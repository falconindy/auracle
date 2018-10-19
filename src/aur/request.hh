#ifndef REQUEST_HH
#define REQUEST_HH

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

#include "package.hh"

namespace aur {

namespace detail {

class Querystring {
 public:
  Querystring() {}

  // We differentiate between "params" and "args" in that "params" are things
  // which should be present across all requests, e.g. v=, type=. "args" are the
  // remaining arg= or arg[]= values.
  void AddParam(const std::string& key, const std::string& value);
  void AddArg(const std::string& key, const std::string& value);

  // Build returns a querystring suitable for appending to a URL. Build should
  // be called repeatedly until it returns an empty string.
  std::string Build();

 private:
  // Upper limit for HTTP/1.1 on aur.archlinux.org is somewhere in the 8000s,
  // but closer to 4k for HTTP2. Let's stick with something that works for both.
  static constexpr int kMaxUriLength = 4000;

  std::map<std::string, std::string> params_;
  std::vector<std::pair<std::string, std::string>> args_;
};

}  // namespace detail

// Abstract class describing an HTTP request to the AUR.
class Request {
 public:
  Request() = default;
  virtual ~Request() = default;

  virtual std::vector<std::string> Build(const std::string& baseurl) const = 0;
};

// A class describing a GET request for an arbitrary URL on the AUR.
class RawRequest : public Request {
 public:
  static std::string UrlForTarball(const Package& package);
  static std::string UrlForPkgbuild(const Package& package);

  explicit RawRequest(std::string urlpath) : urlpath_(std::move(urlpath)) {}

  std::vector<std::string> Build(const std::string& baseurl) const override;

 private:
  const std::string urlpath_;
};

class CloneRequest : public Request {
 public:
  explicit CloneRequest(std::string reponame)
      : reponame_(std::move(reponame)) {}

  const std::string& reponame() const { return reponame_; }

  std::vector<std::string> Build(const std::string& baseurl) const override;

 private:
  const std::string reponame_;
};

// A base class describing a GET request to the RPC endpoint of the AUR.
class RpcRequest : public Request {
 public:
  RpcRequest() { AddParam("v", "5"); }

  std::vector<std::string> Build(const std::string& baseurl) const override;

  void AddArg(const std::string& key, const std::string& value);

 protected:
  void AddParam(const std::string& key, const std::string& value);

 private:
  mutable detail::Querystring qs_;
};

class InfoRequest : public RpcRequest {
 public:
  explicit InfoRequest(const std::vector<std::string>& args) : RpcRequest() {
    AddParam("type", "info");
    for (const auto& arg : args) {
      AddArg(arg);
    }
  }

  InfoRequest() : RpcRequest() { AddParam("type", "info"); }

  void AddArg(const std::string& arg) { RpcRequest::AddArg("arg[]", arg); }
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
    } else if (searchby == "name-desc") {
      return SearchBy::NAME_DESC;
    } else if (searchby == "maintainer") {
      return SearchBy::MAINTAINER;
    } else if (searchby == "depends") {
      return SearchBy::DEPENDS;
    } else if (searchby == "makedepends") {
      return SearchBy::MAKEDEPENDS;
    } else if (searchby == "optdepends") {
      return SearchBy::OPTDEPENDS;
    } else if (searchby == "checkdepends") {
      return SearchBy::CHECKDEPENDS;
    } else {
      return SearchBy::INVALID;
    }
  }

  SearchRequest() : RpcRequest() { AddParam("type", "search"); }

  void SetSearchBy(SearchBy by) { AddParam("by", SearchByToString(by)); }

  void AddArg(const std::string& arg) { RpcRequest::AddArg("arg", arg); }

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

#endif  // REQUEST_HH
