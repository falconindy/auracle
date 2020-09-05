// SPDX-License-Identifier: MIT
#include "aur.hh"

#include <curl/curl.h>
#include <fcntl.h>
#include <systemd/sd-event.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/strip.h"

namespace fs = std::filesystem;

namespace aur {

class AurImpl : public Aur {
 public:
  explicit AurImpl(Options options = Options());
  ~AurImpl() override;

  AurImpl(const AurImpl&) = delete;
  AurImpl& operator=(const AurImpl&) = delete;

  AurImpl(AurImpl&&) = default;
  AurImpl& operator=(AurImpl&&) = default;

  void QueueRpcRequest(const RpcRequest& request,
                       const RpcResponseCallback& callback) override;

  void QueueRawRequest(const HttpRequest& request,
                       const RawResponseCallback& callback) override;

  void QueueCloneRequest(const CloneRequest& request,
                         const CloneResponseCallback& callback) override;

  // Wait for all pending requests to complete. Returns non-zero if any request
  // failed or was cancelled by a callback.
  int Wait() override;

 private:
  using ActiveRequests =
      absl::flat_hash_set<std::variant<CURL*, sd_event_source*>>;

  template <typename ResponseHandlerType>
  void QueueHttpRequest(
      const HttpRequest& request,
      const typename ResponseHandlerType::CallbackType& callback);

  int FinishRequest(CURL* curl, CURLcode result, bool dispatch_callback);
  int FinishRequest(sd_event_source* source);

  int CheckFinished();
  void CancelAll();
  void Cancel(const ActiveRequests::value_type& request);

  enum class DebugLevel {
    // No debugging.
    NONE,

    // Enable Curl's verbose output to stderr
    VERBOSE_STDERR,

    // Enable Curl debug handler, write outbound requests made to a file
    REQUESTS,
  };

  static int SocketCallback(CURLM* curl, curl_socket_t s, int action,
                            void* userdata, void* socketptr);
  int DispatchSocketCallback(curl_socket_t s, int action, sd_event_source* io);

  static int TimerCallback(CURLM* curl, long timeout_ms, void* userdata);
  int DispatchTimerCallback(long timeout_ms);

  static int OnCurlIO(sd_event_source* s, int fd, uint32_t revents,
                      void* userdata);
  static int OnCurlTimer(sd_event_source* s, uint64_t usec, void* userdata);
  static int OnCloneExit(sd_event_source* s, const siginfo_t* si,
                         void* userdata);
  static int OnCancel(sd_event_source* s, void* userdata);

  Options options_;

  CURLM* curl_multi_;
  ActiveRequests active_requests_;

  sigset_t saved_ss_{};
  sd_event* event_ = nullptr;
  sd_event_source* timer_ = nullptr;
  bool cancelled_ = false;

  DebugLevel debug_level_ = DebugLevel::NONE;
  std::ofstream debug_stream_;
};

namespace {

std::string_view GetEnv(const char* name) {
  auto* value = getenv(name);
  return std::string_view(value ? value : "");
}

absl::Status StatusFromCurlHandle(CURL* curl) {
  long http_status;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

  // Most statuses don't need to be specially handled, but some should be
  // classified and/or given a more descriptive message.
  switch (http_status) {
    case 200:
      return absl::OkStatus();
    case 404:
      // Raw requests might result in 404s. Let clients distinguish between this
      // error and others.
      return absl::NotFoundError("Not Found");
    case 429:
      return absl::ResourceExhaustedError(
          "Too many requests: the AUR has throttled your IP for today");
  }

  return absl::InternalError(absl::StrCat("HTTP ", http_status));
}

class ResponseHandler {
 public:
  explicit ResponseHandler(AurImpl* aur) : aur_(aur) {}
  virtual ~ResponseHandler() = default;

  ResponseHandler(const ResponseHandler&) = delete;
  ResponseHandler& operator=(const ResponseHandler&) = delete;

  ResponseHandler(ResponseHandler&&) = default;
  ResponseHandler& operator=(ResponseHandler&&) = default;

  static size_t BodyCallback(char* ptr, size_t size, size_t nmemb,
                             void* userdata) {
    auto* handler = static_cast<ResponseHandler*>(userdata);

    handler->body.append(ptr, size * nmemb);
    return size * nmemb;
  }

  static int DebugCallback(CURL*, curl_infotype type, char* data, size_t size,
                           void* userdata) {
    auto* stream = static_cast<std::ofstream*>(userdata);

    if (type != CURLINFO_HEADER_OUT) {
      return 0;
    }

    stream->write(data, size);
    return 0;
  }

  int RunCallback(absl::Status status) {
    int r = Run(std::move(status));
    delete this;
    return r;
  }

  AurImpl* aur() const { return aur_; }

  std::string body;
  std::array<char, CURL_ERROR_SIZE> error_buffer = {};

 private:
  virtual int Run(absl::Status status) = 0;

  AurImpl* aur_;
};

template <typename ResponseT>
class TypedResponseHandler : public ResponseHandler {
 public:
  using CallbackType = Aur::ResponseCallback<ResponseT>;

  constexpr TypedResponseHandler(AurImpl* aur, CallbackType callback)
      : ResponseHandler(aur), callback_(std::move(callback)) {}

 protected:
  virtual ResponseT MakeResponse() { return ResponseT(std::move(body)); }

  int Run(absl::Status status) override {
    return callback_(ResponseWrapper(MakeResponse(), status));
  }

 private:
  const CallbackType callback_;
};

class RpcResponseHandler : public TypedResponseHandler<RpcResponse> {
 public:
  using TypedResponseHandler<RpcResponse>::TypedResponseHandler;

 protected:
  int Run(absl::Status status) override {
    if (!status.ok()) {
      // The AUR might supply HTML on non-200 replies. We must avoid parsing
      // this as JSON, so drop the response body and trust the callback to do
      // the right thing with the error.
      body.clear();
    }

    return TypedResponseHandler<RpcResponse>::Run(status);
  }
};

using RawResponseHandler = TypedResponseHandler<RawResponse>;

class CloneResponseHandler : public TypedResponseHandler<CloneResponse> {
 public:
  CloneResponseHandler(AurImpl* aur, Aur::CloneResponseCallback callback,
                       std::string operation)
      : TypedResponseHandler(aur, std::move(callback)),
        operation_(std::move(operation)) {}

 protected:
  CloneResponse MakeResponse() override {
    return CloneResponse(std::move(operation_));
  }

 private:
  std::string operation_;
};

}  // namespace

AurImpl::AurImpl(Options options) : options_(std::move(options)) {
  curl_global_init(CURL_GLOBAL_SSL);
  curl_multi_ = curl_multi_init();

  curl_multi_setopt(curl_multi_, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
  curl_multi_setopt(curl_multi_, CURLMOPT_MAX_TOTAL_CONNECTIONS, 5L);

  curl_multi_setopt(curl_multi_, CURLMOPT_SOCKETFUNCTION,
                    &AurImpl::SocketCallback);
  curl_multi_setopt(curl_multi_, CURLMOPT_SOCKETDATA, this);

  curl_multi_setopt(curl_multi_, CURLMOPT_TIMERFUNCTION,
                    &AurImpl::TimerCallback);
  curl_multi_setopt(curl_multi_, CURLMOPT_TIMERDATA, this);

  sigset_t ss{};
  sigaddset(&ss, SIGCHLD);
  sigprocmask(SIG_BLOCK, &ss, &saved_ss_);

  sd_event_default(&event_);

  std::string_view debug = GetEnv("AURACLE_DEBUG");
  if (absl::ConsumePrefix(&debug, "requests:")) {
    debug_level_ = DebugLevel::REQUESTS;
    debug_stream_.open(std::string(debug), std::ofstream::trunc);
  } else if (!debug.empty()) {
    debug_level_ = DebugLevel::VERBOSE_STDERR;
  }
}

AurImpl::~AurImpl() {
  curl_multi_cleanup(curl_multi_);
  curl_global_cleanup();

  sd_event_source_unref(timer_);
  sd_event_unref(event_);

  sigprocmask(SIG_SETMASK, &saved_ss_, nullptr);

  if (debug_stream_.is_open()) {
    debug_stream_.close();
  }
}

void AurImpl::Cancel(const ActiveRequests::value_type& request) {
  struct Visitor {
    constexpr explicit Visitor(AurImpl* aur) : aur(aur) {}

    void operator()(CURL* curl) {
      aur->FinishRequest(curl, CURLE_ABORTED_BY_CALLBACK,
                         /*dispatch_callback=*/false);
    }

    void operator()(sd_event_source* source) { aur->FinishRequest(source); }

    AurImpl* aur;
  };

  std::visit(Visitor(this), request);
}

int AurImpl::OnCancel(sd_event_source*, void* userdata) {
  auto* aur = static_cast<AurImpl*>(userdata);

  while (!aur->active_requests_.empty()) {
    aur->Cancel(*aur->active_requests_.begin());
  }

  aur->cancelled_ = true;

  return 0;
}

void AurImpl::CancelAll() {
  sd_event_source* cancel;
  sd_event_add_defer(event_, &cancel, &AurImpl::OnCancel, this);
  active_requests_.insert(cancel);
}

// static
int AurImpl::SocketCallback(CURLM*, curl_socket_t s, int action, void* userdata,
                            void* sockptr) {
  auto* aur = static_cast<AurImpl*>(userdata);
  auto* io = static_cast<sd_event_source*>(sockptr);
  return aur->DispatchSocketCallback(s, action, io);
}

int AurImpl::DispatchSocketCallback(curl_socket_t s, int action,
                                    sd_event_source* io) {
  if (action == CURL_POLL_REMOVE) {
    sd_event_source_unref(io);
    return CheckFinished();
  }

  auto events = [action]() -> std::uint32_t {
    switch (action) {
      case CURL_POLL_IN:
        return EPOLLIN;
      case CURL_POLL_OUT:
        return EPOLLOUT;
      case CURL_POLL_INOUT:
        return EPOLLIN | EPOLLOUT;
      default:
        return 0;
    }
  }();

  if (io != nullptr) {
    if (sd_event_source_set_io_events(io, events) < 0) {
      return -1;
    }

    if (sd_event_source_set_enabled(io, SD_EVENT_ON) < 0) {
      return -1;
    }
  } else {
    if (sd_event_add_io(event_, &io, s, events, &AurImpl::OnCurlIO, this) < 0) {
      return -1;
    }

    if (curl_multi_assign(curl_multi_, s, io) != CURLM_OK) {
      return -1;
    }
  }

  return 0;
}

// static
int AurImpl::OnCurlIO(sd_event_source*, int fd, uint32_t revents,
                      void* userdata) {
  auto* aur = static_cast<AurImpl*>(userdata);

  int action;
  if ((revents & (EPOLLIN | EPOLLOUT)) == (EPOLLIN | EPOLLOUT)) {
    action = CURL_POLL_INOUT;
  } else if (revents & EPOLLIN) {
    action = CURL_POLL_IN;
  } else if (revents & EPOLLOUT) {
    action = CURL_POLL_OUT;
  } else {
    action = 0;
  }

  int unused;
  if (curl_multi_socket_action(aur->curl_multi_, fd, action, &unused) !=
      CURLM_OK) {
    return -EINVAL;
  }

  return aur->CheckFinished();
}

// static
int AurImpl::OnCurlTimer(sd_event_source*, uint64_t, void* userdata) {
  auto* aur = static_cast<AurImpl*>(userdata);

  int unused;
  if (curl_multi_socket_action(aur->curl_multi_, CURL_SOCKET_TIMEOUT, 0,
                               &unused) != CURLM_OK) {
    return -EINVAL;
  }

  return aur->CheckFinished();
}

// static
int AurImpl::TimerCallback(CURLM*, long timeout_ms, void* userdata) {
  auto* aur = static_cast<AurImpl*>(userdata);
  return aur->DispatchTimerCallback(timeout_ms);
}

int AurImpl::DispatchTimerCallback(long timeout_ms) {
  if (timeout_ms < 0) {
    if (sd_event_source_set_enabled(timer_, SD_EVENT_OFF) < 0) {
      return -1;
    }

    return 0;
  }

  uint64_t usec =
      absl::ToUnixMicros(absl::Now() + absl::Milliseconds(timeout_ms));

  if (timer_ != nullptr) {
    if (sd_event_source_set_time(timer_, usec) < 0) {
      return -1;
    }

    if (sd_event_source_set_enabled(timer_, SD_EVENT_ONESHOT) < 0) {
      return -1;
    }
  } else {
    if (sd_event_add_time(event_, &timer_, CLOCK_REALTIME, usec, 0,
                          &AurImpl::OnCurlTimer, this) < 0) {
      return -1;
    }
  }

  return 0;
}

int AurImpl::FinishRequest(CURL* curl, CURLcode result,
                           bool dispatch_callback) {
  ResponseHandler* handler;
  curl_easy_getinfo(curl, CURLINFO_PRIVATE, &handler);

  int r = 0;
  if (dispatch_callback) {
    absl::Status status =
        result == CURLE_OK ? StatusFromCurlHandle(curl)
                           : absl::UnknownError(handler->error_buffer.data());

    r = handler->RunCallback(std::move(status));
  } else {
    delete handler;
  }

  active_requests_.erase(curl);
  curl_multi_remove_handle(curl_multi_, curl);
  curl_easy_cleanup(curl);

  return r;
}

int AurImpl::FinishRequest(sd_event_source* source) {
  active_requests_.erase(source);
  sd_event_source_unref(source);
  return 0;
}

int AurImpl::CheckFinished() {
  int unused;

  auto* msg = curl_multi_info_read(curl_multi_, &unused);
  if (msg == nullptr || msg->msg != CURLMSG_DONE) {
    return 0;
  }

  auto r = FinishRequest(msg->easy_handle, msg->data.result,
                         /* dispatch_callback = */ true);
  if (r < 0) {
    CancelAll();
  }

  return r;
}

int AurImpl::Wait() {
  cancelled_ = false;

  while (!active_requests_.empty()) {
    if (sd_event_run(event_, 1) < 0) {
      return -EIO;
    }
  }

  return cancelled_ ? -ECANCELED : 0;
}

template <typename ResponseHandlerType>
void AurImpl::QueueHttpRequest(
    const HttpRequest& request,
    const typename ResponseHandlerType::CallbackType& callback) {
  for (const auto& r : request.Build(options_.baseurl)) {
    auto* curl = curl_easy_init();
    auto* handler = new ResponseHandlerType(this, callback);

    using RH = ResponseHandler;
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
    curl_easy_setopt(curl, CURLOPT_URL, r.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &RH::BodyCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, handler);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, handler);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, handler->error_buffer.data());
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, options_.useragent.c_str());

    switch (debug_level_) {
      case DebugLevel::NONE:
        break;
      case DebugLevel::REQUESTS:
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, &RH::DebugCallback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &debug_stream_);
        [[fallthrough]];
      case DebugLevel::VERBOSE_STDERR:
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        break;
    }

    curl_multi_add_handle(curl_multi_, curl);
    active_requests_.emplace(curl);
  }
}

// static
int AurImpl::OnCloneExit(sd_event_source* source, const siginfo_t* si,
                         void* userdata) {
  auto* handler = static_cast<CloneResponseHandler*>(userdata);

  handler->aur()->FinishRequest(source);

  absl::Status status;
  if (si->si_status != 0) {
    status = absl::InternalError(
        absl::StrCat("git exited with unexpected exit status ", si->si_status));
  }

  return handler->RunCallback(std::move(status));
}

void AurImpl::QueueCloneRequest(const CloneRequest& request,
                                const CloneResponseCallback& callback) {
  const bool update = fs::exists(fs::path(request.reponame()) / ".git");

  auto* handler =
      new CloneResponseHandler(this, callback, update ? "update" : "clone");

  int pid = fork();
  if (pid < 0) {
    handler->RunCallback(absl::InternalError(
        absl::StrCat("failed to fork new process for git: ", strerror(errno))));
    return;
  }

  if (pid == 0) {
    const auto url = request.Build(options_.baseurl)[0];

    std::vector<const char*> cmd;
    if (update) {
      // clang-format off
      cmd = {
        "git",
         "-C",
        request.reponame().c_str(),
        "pull",
         "--quiet",
         "--rebase",
         "--autostash",
         "--ff-only",
      };
      // clang-format on
    } else {
      // clang-format off
      cmd = {
        "git",
        "clone",
        "--quiet",
        url.c_str(),
      };
      // clang-format on
    }
    cmd.push_back(nullptr);

    execvp(cmd[0], const_cast<char* const*>(cmd.data()));
    _exit(127);
  }

  sd_event_source* child;
  sd_event_add_child(event_, &child, pid, WEXITED, &AurImpl::OnCloneExit,
                     handler);

  active_requests_.emplace(child);
}

void AurImpl::QueueRawRequest(const HttpRequest& request,
                              const RawResponseCallback& callback) {
  QueueHttpRequest<RawResponseHandler>(request, callback);
}

void AurImpl::QueueRpcRequest(const RpcRequest& request,
                              const RpcResponseCallback& callback) {
  QueueHttpRequest<RpcResponseHandler>(request, callback);
}

std::unique_ptr<Aur> NewAur(Aur::Options options) {
  return std::make_unique<AurImpl>(std::move(options));
}

}  // namespace aur

/* vim: set et ts=2 sw=2: */
