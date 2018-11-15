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
  const auto next_span = [&](const std::string_view& s) {
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

  const auto qs =
      StrJoin(args_.begin(), args_.end(), "&", &QueryParamFormatter);
  std::string_view sv(qs);

  std::vector<std::string> requests;
  while (!sv.empty()) {
    const auto span = next_span(sv);

    requests.push_back(StrCat(baseurl, "/rpc?", baseuri_, "&", span));
    sv.remove_prefix(std::min(sv.length(), span.length() + 1));
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
  return StrCat("/cgit/aur.git/plain/PKGBUILD?h=", UrlEscape(package.pkgbase));
}

std::vector<std::string> RawRequest::Build(const std::string& baseurl) const {
  return {StrCat(baseurl, urlpath_)};
}

std::vector<std::string> CloneRequest::Build(const std::string& baseurl) const {
  return {StrCat(baseurl, "/", reponame_)};
}

}  // namespace aur
