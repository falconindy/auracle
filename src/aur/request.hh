// SPDX-License-Identifier: MIT
#ifndef AUR_REQUEST_HH_
#define AUR_REQUEST_HH_

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "aur/package.hh"

namespace aur {

// Abstract class describing a request for a resource.
class Request {
 public:
  virtual ~Request() = default;

  virtual std::string Url(std::string_view baseurl) const = 0;
};

class HttpRequest : public Request {
 public:
  enum class Command : int8_t {
    GET,
    POST,
  };

  using QueryParam = std::pair<std::string, std::string>;
  using QueryParams = std::vector<QueryParam>;

  explicit HttpRequest(Command command) : command_(command) {}

  Command command() const { return command_; }

  virtual std::string Payload() const = 0;

 protected:
  Command command_;
};

class RpcRequest : public HttpRequest {
 public:
  RpcRequest(Command command, std::string endpoint)
      : HttpRequest(command), endpoint_(std::move(endpoint)) {}

  std::string Url(std::string_view baseurl) const override;
  std::string Payload() const override;

  void AddArg(std::string key, std::string value);

 private:
  std::string endpoint_;
  QueryParams params_;
};

// A class describing a GET request for an arbitrary URL on the AUR.
class RawRequest : public HttpRequest {
 public:
  static RawRequest ForSourceFile(const Package& package,
                                  std::string_view filename);

  explicit RawRequest(std::string urlpath)
      : HttpRequest(HttpRequest::Command::GET), urlpath_(std::move(urlpath)) {}

  RawRequest(const RawRequest&) = delete;
  RawRequest& operator=(const RawRequest&) = delete;

  RawRequest(RawRequest&&) = default;
  RawRequest& operator=(RawRequest&&) = default;

  std::string Url(std::string_view baseurl) const override;
  std::string Payload() const override { return std::string(); }

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

  std::string Url(std::string_view baseurl) const override;

 private:
  std::string reponame_;
};

class InfoRequest : public RpcRequest {
 public:
  explicit InfoRequest(const std::vector<std::string>& args) : InfoRequest() {
    for (const auto& arg : args) {
      AddArg(arg);
    }
  }

  explicit InfoRequest(const std::vector<Package>& packages) : InfoRequest() {
    for (const auto& package : packages) {
      AddArg(package.name);
    }
  }

  InfoRequest(const InfoRequest&) = delete;
  InfoRequest& operator=(const InfoRequest&) = delete;

  InfoRequest(InfoRequest&&) = default;
  InfoRequest& operator=(InfoRequest&&) = default;

  InfoRequest() : RpcRequest(HttpRequest::Command::POST, "/rpc/v5/info") {}

  void AddArg(std::string arg) { RpcRequest::AddArg("arg[]", std::move(arg)); }
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
    SUBMITTER,
    PROVIDES,
    CONFLICTS,
    REPLACES,
    KEYWORDS,
    GROUPS,
    COMAINTAINERS,
  };

  static SearchBy ParseSearchBy(std::string_view searchby);

  SearchRequest(SearchBy by, std::string_view arg);

  SearchRequest(const SearchRequest&) = delete;
  SearchRequest& operator=(const SearchRequest&) = delete;

  SearchRequest(SearchRequest&&) = default;
  SearchRequest& operator=(SearchRequest&&) = default;

 private:
  std::string SearchByToString(SearchBy by);
};

}  // namespace aur

#endif  // AUR_REQUEST_HH_
