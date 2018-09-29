#include "request.hh"

#include <memory>
#include <sstream>
#include <string_view>

#include <curl/curl.h>
#include <stdio.h>

namespace aur {

namespace {

using QueryParam = std::pair<std::string, std::string>;

class EncodedParam {
 public:
  EncodedParam(const QueryParam& kv) : kv_(kv) {}

  friend std::ostream& operator<<(std::ostream& os, const EncodedParam& param) {
    std::unique_ptr<char> escaped(curl_easy_escape(
        nullptr, param.kv_.second.data(), param.kv_.second.size()));

    return os << param.kv_.first << '=' << escaped.get();
  }

 private:
  const QueryParam& kv_;
};

}  // namespace

namespace detail {

void Querystring::AddParam(const std::string& key, const std::string& value) {
  params_.emplace(key, value);
}

void Querystring::AddArg(const std::string& key, const std::string& value) {
  args_.emplace_back(key, value);
}

std::string Querystring::Build() {
  std::stringstream ss;

  if (args_.empty()) {
    return std::string();
  }

  for (auto it = params_.cbegin(); it != params_.cend(); ++it) {
    if (it != params_.cbegin()) {
      ss << '&';
    }

    ss << EncodedParam(*it);
  }

  auto ss_size = [](std::stringstream& ss) -> size_t {
    ss.seekp(0, std::ios::end);
    auto size = ss.tellp();
    ss.seekg(0, std::ios::beg);
    return size;
  };

  auto param_size = [](const QueryParam& kv) {
    // Size of key, plus '=', and worst case scenario for value size.
    return kv.first.size() + 1 + kv.second.size() * 3;
  };

  for (auto it = args_.cbegin(); it != args_.cend();) {
    if (ss_size(ss) + param_size(*it) > kMaxUriLength) {
      break;
    }

    ss << '&' << EncodedParam(*it);
    it = args_.erase(it);
  }

  return ss.str();
}

}  // namespace detail

void RpcRequest::AddParam(const std::string& key, const std::string& value) {
  qs_.AddParam(key, value);
}

void RpcRequest::AddArg(const std::string& key, const std::string& value) {
  qs_.AddArg(key, value);
}

std::vector<std::string> RpcRequest::Build(const std::string& baseurl) const {
  std::vector<std::string> requests;

  for (;;) {
    auto args = qs_.Build();
    if (args.empty()) {
      break;
    }

    std::stringstream ss;

    ss << baseurl << "/rpc?" << args;

    requests.push_back(ss.str());
  }

  return requests;
}

// static
std::string RawRequest::UrlForTarball(const Package& package) {
  return package.aur_urlpath;
}

// static
std::string RawRequest::UrlForPkgbuild(const Package& package) {
  return "/cgit/aur.git/plain/PKGBUILD?h=" + package.pkgbase;
}

std::vector<std::string> RawRequest::Build(const std::string& baseurl) const {
  std::stringstream ss;

  ss << baseurl << urlpath_;

  return {ss.str()};
}


}  // namespace aur
