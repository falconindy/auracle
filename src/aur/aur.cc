#include "aur.hh"

#include <assert.h>

#include <iostream>
#include <memory>
#include <string_view>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

namespace aur {

namespace {

struct ci_char_traits : public std::char_traits<char> {
  static bool eq(char c1, char c2) { return toupper(c1) == toupper(c2); }
  static bool ne(char c1, char c2) { return toupper(c1) != toupper(c2); }
  static bool lt(char c1, char c2) { return toupper(c1) < toupper(c2); }
  static int compare(const char* s1, const char* s2, size_t n) {
    while (n-- != 0) {
      if (toupper(*s1) < toupper(*s2)) {
        return -1;
      }
      if (toupper(*s1) > toupper(*s2)) {
        return 1;
      }
      ++s1;
      ++s2;
    }
    return 0;
  }
  static const char* find(const char* s, int n, char a) {
    while (n-- > 0 && toupper(*s) != toupper(a)) {
      ++s;
    }
    return s;
  }
};

using ci_string_view = std::basic_string_view<char, ci_char_traits>;

bool ConsumePrefix(ci_string_view* view, ci_string_view prefix) {
  if (view->find(prefix) != 0) {
    return false;
  }

  view->remove_prefix(prefix.size());
  return true;
}

struct ResponseHandler {
  virtual ~ResponseHandler() = default;

  static size_t DataCallback(char* ptr, size_t size, size_t nmemb,
                             void* userdata) {
    ResponseHandler* const handler = static_cast<ResponseHandler*>(userdata);

    handler->body.append(ptr, size * nmemb);

    return size * nmemb;
  }

  static size_t HeaderCallback(char* buffer, size_t size, size_t nitems,
                               void* userdata) {
    ResponseHandler* const handler = static_cast<ResponseHandler*>(userdata);

    // Remove 2 bytes to ignore trailing \r\n
    ci_string_view header(buffer, size * nitems - 2);

    if (ConsumePrefix(&header, "Content-Disposition:")) {
      handler->FilenameFromHeader(header);
    }

    return size * nitems;
  }

  virtual int RunCallback(const std::string& error) const = 0;

  std::string body;
  std::string content_disposition_filename;
  char error_buffer[CURL_ERROR_SIZE]{};

 private:
  void FilenameFromHeader(ci_string_view value) {
    constexpr char kFilenameKey[] = "filename=";

    auto pos = value.find(kFilenameKey);
    if (pos == std::string_view::npos) {
      return;
    }

    value.remove_prefix(pos + strlen(kFilenameKey));
    if (value.size() < 3) {
      // Malformed header or empty filename
      return;
    }

    // Blind assumptions:
    // 1) filename is either quoted or the last
    // 2) filename is never a path (contains no slashes)
    ConsumePrefix(&value, "\"");
    content_disposition_filename = std::string(value.data(), value.find('"'));
  }
};

struct RpcResponseHandler : public ResponseHandler {
  RpcResponseHandler(Aur::RpcResponseCallback cb_) : cb(std::move(cb_)) {}

  int RunCallback(const std::string& error) const override {
    if (!error.empty()) {
      return cb(error);
    }

    auto json = aur::RpcResponse::Parse(body);
    if (!json.error.empty()) {
      return cb(json.error);
    }

    return cb(std::move(json));
  }

  Aur::RpcResponseCallback cb;
};

struct DownloadResponseHandler : public ResponseHandler {
  DownloadResponseHandler(Aur::DownloadResponseCallback cb_)
      : cb(std::move(cb_)) {}

  int RunCallback(const std::string& error) const override {
    if (error.empty()) {
      return cb(DownloadResponse{std::move(content_disposition_filename),
                                 std::move(body)});
    } else {
      return cb(error);
    }
  }

  Aur::DownloadResponseCallback cb;
};

}  // namespace

Aur::Aur(const std::string& baseurl) : baseurl_(baseurl) {
  assert(curl_global_init(CURL_GLOBAL_SSL) == 0);
  curl_multi_ = curl_multi_init();
  curl_multi_setopt(curl_multi_, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
}

Aur::~Aur() {
  curl_multi_cleanup(curl_multi_);
  curl_global_cleanup();
}

void Aur::StartRequest(CURL* curl) {
  curl_multi_add_handle(curl_multi_, curl);
  active_requests_.insert(curl);
}

int Aur::FinishRequest(CURL* curl, CURLcode result, bool dispatch_callback) {
  ResponseHandler* handler;
  curl_easy_getinfo(curl, CURLINFO_PRIVATE, &handler);

  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  std::string error = handler->error_buffer;
  if (error.empty()) {
    if (result == CURLE_OK) {
      if (response_code != 200) {
        error = "HTTP " + std::to_string(response_code);
      }
    } else {
      error = curl_easy_strerror(result);
    }
  }

  auto r = dispatch_callback ? handler->RunCallback(error) : 0;
  delete handler;

  active_requests_.erase(curl);
  curl_multi_remove_handle(curl_multi_, curl);
  curl_easy_cleanup(curl);

  return r;
}

int Aur::ProcessDoneEvents() {
  for (;;) {
    int msgs_left;
    auto msg = curl_multi_info_read(curl_multi_, &msgs_left);
    if (msg == NULL) {
      break;
    }

    if (msg->msg != CURLMSG_DONE) {
      continue;
    }

    auto r = FinishRequest(msg->easy_handle, msg->data.result,
                           /* dispatch_callback = */ true);
    if (r != 0) {
      return r;
    }
  }

  return 0;
}

void Aur::Cancel() {
  while (active_requests_.size() > 0) {
    FinishRequest(*active_requests_.begin(), CURLE_ABORTED_BY_CALLBACK,
                  /* dispatch_callback = */ false);
  }
}

int Aur::Wait() {
  while (!active_requests_.empty()) {
    int nfd, rc = curl_multi_wait(curl_multi_, NULL, 0, 1000, &nfd);
    if (rc != CURLM_OK) {
      fprintf(stderr, "error: curl_multi_wait failed (%d)\n", rc);
      return 1;
    }

    if (nfd < 0) {
      fprintf(stderr, "error: poll error, possible network problem\n");
      return 1;
    }

    int active;
    rc = curl_multi_perform(curl_multi_, &active);
    if (rc != CURLM_OK) {
      fprintf(stderr, "error: curl_multi_perform failed (%d)\n", rc);
      return 1;
    }

    auto r = ProcessDoneEvents();
    if (r != 0) {
      Cancel();
      return r;
    }
  }

  return 0;
}

struct RpcRequestTraits {
  enum : bool { kNeedHeaders = false };

  using CallbackType = Aur::RpcResponseCallback;
  using ResponseHandlerType = RpcResponseHandler;

  static constexpr char const* kEncoding = "";
};

struct DownloadRequestTraits {
  enum : bool { kNeedHeaders = true };

  using CallbackType = Aur::DownloadResponseCallback;
  using ResponseHandlerType = DownloadResponseHandler;

  static constexpr char const* kEncoding = "identity";
};

template <typename RequestType>
void Aur::QueueRequest(const Request* request,
                       const typename RequestType::CallbackType& callback) {
  for (const auto& r : request->Build(baseurl_)) {
    auto curl = curl_easy_init();

    auto response_handler =
        new typename RequestType::ResponseHandlerType(callback);

    if (getenv("AURACLE_DEBUG")) {
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    using RH = ResponseHandler;
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
    curl_easy_setopt(curl, CURLOPT_URL, r.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &RH::DataCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_handler);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, response_handler);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, response_handler->error_buffer);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, RequestType::kEncoding);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Auracle/0");

    if (RequestType::kNeedHeaders) {
      curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &RH::HeaderCallback);
      curl_easy_setopt(curl, CURLOPT_HEADERDATA, response_handler);
    }

    StartRequest(curl);
  }
}

void Aur::QueueRpcRequest(const RpcRequest* request,
                          const RpcResponseCallback& callback) {
  QueueRequest<RpcRequestTraits>(request, callback);
}

void Aur::QueueDownloadRequest(const DownloadRequest* request,
                               const DownloadResponseCallback& callback) {
  QueueRequest<DownloadRequestTraits>(request, callback);
}

void Aur::SetConnectTimeout(long timeout) { connect_timeout_ = timeout; }

void Aur::SetMaxConnections(long connections) {
  curl_multi_setopt(curl_multi_, CURLMOPT_MAX_TOTAL_CONNECTIONS, connections);
}

}  // namespace aur

/* vim: set et ts=2 sw=2: */
