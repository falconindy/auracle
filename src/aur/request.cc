#include "request.hh"

#include <cstring>
#include <memory>
#include <string_view>

#include <curl/curl.h>

namespace aur {

namespace {

std::string UrlEscape(const std::string_view sv) {
  char* ptr = curl_easy_escape(nullptr, sv.data(), sv.size());
  std::string escaped(ptr);
  free(ptr);

  return escaped;
}

char* AppendUnsafe(char* to, std::string_view from) {
  auto ptr = mempcpy(to, from.data(), from.size());

  return static_cast<char*>(ptr);
}

template <typename... Pieces>
void StrAppend(std::string* out, const Pieces&... args) {
  std::vector<std::string_view> v{args...};

  std::string::size_type append_sz = 0;
  for (const auto& piece : v) {
    append_sz += piece.size();
  }

  auto original_sz = out->size();
  out->resize(original_sz + append_sz);

  char* ptr = out->data() + original_sz;
  for (const auto& piece : v) {
    ptr = AppendUnsafe(ptr, piece);
  }
}

template <typename... Pieces>
std::string StrCat(const Pieces&... args) {
  std::string out;
  StrAppend(&out, args...);
  return out;
}

void QueryParamFormatter(std::string* out,
                         const std::pair<std::string, std::string>& kv) {
  StrAppend(out, kv.first, "=", UrlEscape(kv.second));
}

template <typename Iterator, typename Formatter>
std::string StrJoin(Iterator begin, Iterator end, std::string_view s,
                    Formatter&& f) {
  std::string result;

  std::string_view sep("");
  for (Iterator it = begin; it != end; ++it) {
    result.append(sep.data(), sep.size());
    f(&result, *it);
    sep = s;
  }

  return result;
}

}  // namespace

void RpcRequest::AddArg(const std::string& key, const std::string& value) {
  args_.emplace_back(key, value);
}

std::vector<std::string> RpcRequest::Build(const std::string& baseurl) const {
  std::vector<std::string> requests;

  const auto assemble = [&](std::string_view qs) {
    return StrCat(baseurl, "/rpc?", baseuri_, "&", qs);
  };

  const auto qs =
      StrJoin(args_.begin(), args_.end(), "&", &QueryParamFormatter);

  std::string_view sv(qs);
  while (!sv.empty()) {
    if (sv.size() <= approx_max_length_) {
      requests.push_back(assemble(sv));
      break;
    }

    std::string_view::size_type n = approx_max_length_;
    do {
      n = sv.substr(0, n).find_last_of("&");
    } while (n > kMaxUriLength);

    requests.push_back(assemble(sv.substr(0, n)));
    sv.remove_prefix(n + 1);
  }

  return requests;
}

RpcRequest::RpcRequest(
    const std::vector<std::pair<std::string, std::string>>& base_params,
    long unsigned approx_max_length)
    : baseuri_(StrJoin(base_params.begin(), base_params.end(), "&",
                       &QueryParamFormatter)),
      approx_max_length_(approx_max_length) {}

// static
std::string RawRequest::UrlForTarball(const Package& package) {
  return package.aur_urlpath;
}

// static
std::string RawRequest::UrlForPkgbuild(const Package& package) {
  return "/cgit/aur.git/plain/PKGBUILD?h=" + package.pkgbase;
}

std::vector<std::string> RawRequest::Build(const std::string& baseurl) const {
  return {StrCat(baseurl, urlpath_)};
}

std::vector<std::string> CloneRequest::Build(const std::string& baseurl) const {
  return {StrCat(baseurl, "/", reponame_)};
}

}  // namespace aur
