// SPDX-License-Identifier: MIT
#include "aur/client.hh"

#include <curl/curl.h>
#include <systemd/sd-event.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"

namespace fs = std::filesystem;

namespace aur {

class ClientImpl : public Client {
 public:
  explicit ClientImpl(Client::Options options = Options());
  ~ClientImpl() override;

  ClientImpl(const ClientImpl&) = delete;
  ClientImpl& operator=(const ClientImpl&) = delete;

  ClientImpl(ClientImpl&&) = default;
  ClientImpl& operator=(ClientImpl&&) = default;

  void QueueRpcRequest(const RpcRequest& request,
                       RpcResponseCallback callback) override;

  void QueueRawRequest(const HttpRequest& request,
                       RawResponseCallback callback) override;

  void QueueCloneRequest(const CloneRequest& request,
                         CloneResponseCallback callback) override;

  // Wait for all pending requests to complete. Returns non-zero if any request
  // failed or was cancelled by a callback.
  int Wait() override;

 private:
  using ActiveRequests =
      absl::flat_hash_set<std::variant<CURL*, sd_event_source*>>;

  template <typename ResponseHandlerType>
  void QueueHttpRequest(const HttpRequest& request,
                        ResponseHandlerType::CallbackType callback);

  template <typename ResponseHandlerType>
  void QueueRpcRequest(const RpcRequest& request,
                       ResponseHandlerType::CallbackType callback);

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
  const auto* value = getenv(name);
  return value ? std::string_view(value) : std::string_view();
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
          "Too many requests: the AUR has throttled your IP.");
  }

  return absl::InternalError(absl::StrCat("HTTP ", http_status));
}

class ResponseHandler {
 public:
  explicit ResponseHandler(ClientImpl* client) : client_(client) {}
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

  int Finalize(absl::Status status) {
    int r = RunCallback(std::move(status));
    delete this;
    return r;
  }

  ClientImpl* client() const { return client_; }

  std::string body;
  std::array<char, CURL_ERROR_SIZE> error_buffer = {};

 private:
  virtual int RunCallback(absl::Status status) = 0;

  ClientImpl* client_;
};

template <typename ResponseT>
class TypedResponseHandler : public ResponseHandler {
 public:
  using CallbackType = Client::ResponseCallback<ResponseT>;

  constexpr TypedResponseHandler(ClientImpl* client, CallbackType callback)
      : ResponseHandler(client), callback_(std::move(callback)) {}

 protected:
  int RunCallback(absl::Status status) override {
    if (!status.ok()) {
      return std::move(callback_)(std::move(status));
    }

    return std::move(callback_)(ResponseT::Parse(std::move(body)));
  }

 private:
  CallbackType callback_;
};

class RpcResponseHandler : public TypedResponseHandler<RpcResponse> {
 public:
  using TypedResponseHandler<RpcResponse>::TypedResponseHandler;

 protected:
  int RunCallback(absl::Status status) override {
    if (!status.ok()) {
      // The AUR might supply HTML on non-200 replies. We must avoid parsing
      // this as JSON, so drop the response body and trust the callback to do
      // the right thing with the error.
      body.clear();
    }

    return TypedResponseHandler<RpcResponse>::RunCallback(std::move(status));
  }
};

using RawResponseHandler = TypedResponseHandler<RawResponse>;

class CloneResponseHandler : public TypedResponseHandler<CloneResponse> {
 public:
  CloneResponseHandler(ClientImpl* client,
                       Client::CloneResponseCallback callback,
                       std::string_view operation)
      : TypedResponseHandler(client, std::move(callback)) {
    body = operation;
  }
};

}  // namespace

ClientImpl::ClientImpl(Options options) : options_(std::move(options)) {
  curl_global_init(CURL_GLOBAL_SSL);
  curl_multi_ = curl_multi_init();

  curl_multi_setopt(curl_multi_, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
  curl_multi_setopt(curl_multi_, CURLMOPT_MAX_TOTAL_CONNECTIONS, 5L);

  curl_multi_setopt(curl_multi_, CURLMOPT_SOCKETFUNCTION,
                    &ClientImpl::SocketCallback);
  curl_multi_setopt(curl_multi_, CURLMOPT_SOCKETDATA, this);

  curl_multi_setopt(curl_multi_, CURLMOPT_TIMERFUNCTION,
                    &ClientImpl::TimerCallback);
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

ClientImpl::~ClientImpl() {
  curl_multi_cleanup(curl_multi_);
  curl_global_cleanup();

  sd_event_source_unref(timer_);
  sd_event_unref(event_);

  sigprocmask(SIG_SETMASK, &saved_ss_, nullptr);

  if (debug_stream_.is_open()) {
    debug_stream_.close();
  }
}

void ClientImpl::Cancel(const ActiveRequests::value_type& request) {
  std::visit(absl::Overload{
                 [this](CURL* curl) {
                   FinishRequest(curl, CURLE_ABORTED_BY_CALLBACK,
                                 /*dispatch_callback=*/false);
                 },
                 [this](sd_event_source* source) { FinishRequest(source); }},
             request);
}

int ClientImpl::OnCancel(sd_event_source*, void* userdata) {
  auto* client = static_cast<ClientImpl*>(userdata);

  while (!client->active_requests_.empty()) {
    client->Cancel(*client->active_requests_.begin());
  }

  return 0;
}

void ClientImpl::CancelAll() {
  if (cancelled_) {
    return;
  }

  cancelled_ = true;

  sd_event_source* cancel;
  sd_event_add_defer(event_, &cancel, &ClientImpl::OnCancel, this);
  active_requests_.insert(cancel);
}

// static
int ClientImpl::SocketCallback(CURLM*, curl_socket_t s, int action,
                               void* userdata, void* sockptr) {
  auto* client = static_cast<ClientImpl*>(userdata);
  auto* io = static_cast<sd_event_source*>(sockptr);
  return client->DispatchSocketCallback(s, action, io);
}

int ClientImpl::DispatchSocketCallback(curl_socket_t s, int action,
                                       sd_event_source* io) {
  if (action == CURL_POLL_REMOVE) {
    sd_event_source_unref(io);
    return CheckFinished();
  }

  uint32_t events = 0;
  if (action & CURL_CSELECT_IN) {
    events |= EPOLLIN;
  }
  if (action & CURL_CSELECT_OUT) {
    events |= EPOLLOUT;
  }

  if (io != nullptr) {
    if (sd_event_source_set_io_events(io, events) < 0) {
      return -1;
    }

    if (sd_event_source_set_enabled(io, SD_EVENT_ON) < 0) {
      return -1;
    }
  } else {
    if (sd_event_add_io(event_, &io, s, events, &ClientImpl::OnCurlIO, this) <
        0) {
      return -1;
    }

    if (curl_multi_assign(curl_multi_, s, io) != CURLM_OK) {
      return -1;
    }
  }

  return 0;
}

// static
int ClientImpl::OnCurlIO(sd_event_source*, int fd, uint32_t revents,
                         void* userdata) {
  auto* client = static_cast<ClientImpl*>(userdata);

  int action = 0;
  if (revents & EPOLLIN) {
    action |= CURL_CSELECT_IN;
  }
  if (revents & EPOLLOUT) {
    action |= CURL_CSELECT_OUT;
  }

  int unused;
  if (curl_multi_socket_action(client->curl_multi_, fd, action, &unused) !=
      CURLM_OK) {
    return -EINVAL;
  }

  return client->CheckFinished();
}

// static
int ClientImpl::OnCurlTimer(sd_event_source*, uint64_t, void* userdata) {
  auto* client = static_cast<ClientImpl*>(userdata);

  int unused;
  if (curl_multi_socket_action(client->curl_multi_, CURL_SOCKET_TIMEOUT, 0,
                               &unused) != CURLM_OK) {
    return -EINVAL;
  }

  return client->CheckFinished();
}

// static
int ClientImpl::TimerCallback(CURLM*, long timeout_ms, void* userdata) {
  auto* client = static_cast<ClientImpl*>(userdata);
  return client->DispatchTimerCallback(timeout_ms);
}

int ClientImpl::DispatchTimerCallback(long timeout_ms) {
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
                          &ClientImpl::OnCurlTimer, this) < 0) {
      return -1;
    }
  }

  return 0;
}

int ClientImpl::FinishRequest(CURL* curl, CURLcode result,
                              bool dispatch_callback) {
  ResponseHandler* handler;
  curl_easy_getinfo(curl, CURLINFO_PRIVATE, &handler);

  int r = 0;
  if (dispatch_callback) {
    absl::Status status =
        result == CURLE_OK ? StatusFromCurlHandle(curl)
                           : absl::UnknownError(handler->error_buffer.data());

    r = handler->Finalize(std::move(status));
  } else {
    delete handler;
  }

  active_requests_.erase(curl);
  curl_multi_remove_handle(curl_multi_, curl);
  curl_easy_cleanup(curl);

  return r;
}

int ClientImpl::FinishRequest(sd_event_source* source) {
  active_requests_.erase(source);
  sd_event_source_unref(source);
  return 0;
}

int ClientImpl::CheckFinished() {
  int unused;

  int r = 0;
  while (true) {
    auto* msg = curl_multi_info_read(curl_multi_, &unused);
    if (msg == nullptr || msg->msg != CURLMSG_DONE) {
      break;
    }

    r = FinishRequest(msg->easy_handle, msg->data.result,
                      /* dispatch_callback = */ true);
    if (r < 0) {
      CancelAll();
      break;
    }
  }

  return r;
}

int ClientImpl::Wait() {
  cancelled_ = false;

  while (!active_requests_.empty()) {
    if (sd_event_run(event_, 10000) < 0) {
      return -EIO;
    }
  }

  return cancelled_ ? -ECANCELED : 0;
}

template <typename ResponseHandlerType>
void ClientImpl::QueueHttpRequest(const HttpRequest& request,
                                  ResponseHandlerType::CallbackType callback) {
  auto* curl = curl_easy_init();
  auto* handler = new ResponseHandlerType(this, std::move(callback));

  using RH = ResponseHandler;
  curl_easy_setopt(curl, CURLOPT_URL, request.Url(options_.baseurl).c_str());
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, options_.useragent.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &RH::BodyCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, handler);
  curl_easy_setopt(curl, CURLOPT_PRIVATE, handler);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, handler->error_buffer.data());

  if (request.command() == RpcRequest::Command::POST) {
    curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, request.Payload().c_str());
  }

  switch (debug_level_) {
    case DebugLevel::NONE:
      break;
    case DebugLevel::REQUESTS:
      curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION,
                       &ResponseHandler::DebugCallback);
      curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &debug_stream_);
      [[fallthrough]];
    case DebugLevel::VERBOSE_STDERR:
      curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      break;
  }

  curl_multi_add_handle(curl_multi_, curl);
  active_requests_.emplace(curl);
}

// static
int ClientImpl::OnCloneExit(sd_event_source* source, const siginfo_t* si,
                            void* userdata) {
  auto* handler = static_cast<CloneResponseHandler*>(userdata);

  handler->client()->FinishRequest(source);

  absl::Status status;
  if (si->si_status != 0) {
    status = absl::InternalError(
        absl::StrCat("git exited with unexpected exit status ", si->si_status));
  }

  return handler->Finalize(std::move(status));
}

void ClientImpl::QueueCloneRequest(const CloneRequest& request,
                                   CloneResponseCallback callback) {
  const bool update = fs::exists(fs::path(request.reponame()) / ".git");

  auto* handler = new CloneResponseHandler(this, std::move(callback),
                                           update ? "update" : "clone");

  int pid = fork();
  if (pid < 0) {
    handler->Finalize(absl::InternalError(
        absl::StrCat("failed to fork new process for git: ", strerror(errno))));
    return;
  }

  if (pid == 0) {
    const auto url = request.Url(options_.baseurl);

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
  sd_event_add_child(event_, &child, pid, WEXITED, &ClientImpl::OnCloneExit,
                     handler);

  active_requests_.emplace(child);
}

void ClientImpl::QueueRawRequest(const HttpRequest& request,
                                 RawResponseCallback callback) {
  QueueHttpRequest<RawResponseHandler>(request, std::move(callback));
}

void ClientImpl::QueueRpcRequest(const RpcRequest& request,
                                 RpcResponseCallback callback) {
  QueueHttpRequest<RpcResponseHandler>(request, std::move(callback));
}

std::unique_ptr<Client> Client::New(Client::Options options) {
  return std::make_unique<ClientImpl>(std::move(options));
}

}  // namespace aur

/* vim: set et ts=2 sw=2: */
