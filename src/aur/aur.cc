#include "aur.hh"

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

namespace aur {

namespace {

template <typename TimeRes, typename ClockType>
auto TimepointTo(std::chrono::time_point<ClockType> tp) {
  return std::chrono::time_point_cast<TimeRes>(tp).time_since_epoch().count();
}

bool ConsumePrefix(std::string_view* view, std::string_view prefix) {
  if (view->find(prefix) != 0) {
    return false;
  }

  view->remove_prefix(prefix.size());
  return true;
}

class ResponseHandler {
 public:
  ResponseHandler() = default;
  virtual ~ResponseHandler() = default;

  ResponseHandler(const ResponseHandler&) = delete;
  ResponseHandler& operator=(const ResponseHandler&) = delete;

  ResponseHandler(ResponseHandler&&) = default;
  ResponseHandler& operator=(ResponseHandler&&) = default;

  static size_t BodyCallback(char* ptr, size_t size, size_t nmemb,
                             void* userdata) {
    auto handler = static_cast<ResponseHandler*>(userdata);

    handler->body.append(ptr, size * nmemb);
    return size * nmemb;
  }

  static int DebugCallback(CURL*, curl_infotype type, char* data, size_t size,
                           void* userdata) {
    auto stream = static_cast<std::ofstream*>(userdata);

    if (type != CURLINFO_HEADER_OUT) {
      return 0;
    }

    stream->write(data, size);
    return 0;
  }

  int RunCallback(const std::string& error) {
    int r = Run(error);
    delete this;
    return r;
  }

  std::string body;
  char error_buffer[CURL_ERROR_SIZE]{};

 private:
  virtual int Run(const std::string& error) = 0;
};

template <typename ResponseT, typename CallbackT>
class TypedResponseHandler : public ResponseHandler {
 public:
  using CallbackType = CallbackT;

  TypedResponseHandler(Aur* aur, CallbackT callback)
      : aur_(aur), callback_(std::move(callback)) {}

  Aur* aur() const { return aur_; }

 protected:
  virtual ResponseT MakeResponse() { return ResponseT(std::move(body)); }

 private:
  int Run(const std::string& error) override {
    if (!error.empty()) {
      return callback_(error);
    }

    return callback_(MakeResponse());
  }

  Aur* aur_;
  const CallbackT callback_;
};

using RpcResponseHandler =
    TypedResponseHandler<RpcResponse, Aur::RpcResponseCallback>;
using RawResponseHandler =
    TypedResponseHandler<RawResponse, Aur::RawResponseCallback>;

class CloneResponseHandler
    : public TypedResponseHandler<CloneResponse, Aur::CloneResponseCallback> {
 public:
  using TypedResponseHandler<CloneResponse,
                             Aur::CloneResponseCallback>::TypedResponseHandler;

  void SetOperation(std::string operation) {
    operation_ = std::move(operation);
  }

  CloneResponse MakeResponse() override { return {std::move(operation_)}; }

 private:
  std::string operation_;
};

}  // namespace

Aur::Aur(std::string baseurl) : baseurl_(std::move(baseurl)) {
  curl_global_init(CURL_GLOBAL_SSL);
  curl_ = curl_multi_init();

  curl_multi_setopt(curl_, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

  curl_multi_setopt(curl_, CURLMOPT_SOCKETFUNCTION, &Aur::SocketCallback);
  curl_multi_setopt(curl_, CURLMOPT_SOCKETDATA, this);

  curl_multi_setopt(curl_, CURLMOPT_TIMERFUNCTION, &Aur::TimerCallback);
  curl_multi_setopt(curl_, CURLMOPT_TIMERDATA, this);

  sigset_t ss{};
  sigaddset(&ss, SIGCHLD);
  sigprocmask(SIG_BLOCK, &ss, &saved_ss_);

  sd_event_default(&event_);

  std::string_view debug(getenv("AURACLE_DEBUG"));
  if (ConsumePrefix(&debug, "requests:")) {
    debug_level_ = DEBUG_REQUESTS;
    debug_stream_.open(std::string(debug), std::ofstream::trunc);
  } else if (!debug.empty()) {
    debug_level_ = DEBUG_VERBOSE_STDERR;
  }
}

Aur::~Aur() {
  curl_multi_cleanup(curl_);
  curl_global_cleanup();

  sd_event_source_unref(timer_);
  sd_event_unref(event_);

  sigprocmask(SIG_SETMASK, &saved_ss_, nullptr);

  if (debug_stream_.is_open()) {
    debug_stream_.close();
  }
}

void Aur::Cancel(const ActiveRequests::value_type& request) {
  struct Visitor {
    explicit Visitor(Aur* aur) : aur(aur) {}

    void operator()(CURL* curl) {
      aur->FinishRequest(curl, CURLE_ABORTED_BY_CALLBACK,
                         /*dispatch_callback=*/false);
    }

    void operator()(sd_event_source* source) { aur->FinishRequest(source); }

    Aur* aur;
  };

  std::visit(Visitor(this), request);
}

void Aur::CancelAll() {
  while (!active_requests_.empty()) {
    Cancel(*active_requests_.begin());
  }

  sd_event_exit(event_, 1);
}

// static
int Aur::SocketCallback(CURLM*, curl_socket_t s, int action, void* userdata,
                        void*) {
  auto aur = static_cast<Aur*>(userdata);

  auto iter = aur->active_io_.find(s);
  sd_event_source* io = iter != aur->active_io_.end() ? iter->second : nullptr;

  if (action == CURL_POLL_REMOVE) {
    if (io != nullptr) {
      int fd = sd_event_source_get_io_fd(io);

      sd_event_source_set_enabled(io, SD_EVENT_OFF);
      sd_event_source_unref(io);

      aur->active_io_.erase(iter);
      aur->translate_fds_.erase(fd);

      close(fd);
    }
  }

  auto action_to_revents = [](int action) -> std::uint32_t {
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
  };
  std::uint32_t events = action_to_revents(action);

  if (iter != aur->active_io_.end()) {
    if (sd_event_source_set_io_events(io, events) < 0) {
      return -1;
    }

    if (sd_event_source_set_enabled(io, SD_EVENT_ON) < 0) {
      return -1;
    }
  } else {
    /* When curl needs to remove an fd from us it closes
     * the fd first, and only then calls into us. This is
     * nasty, since we cannot pass the fd on to epoll()
     * anymore. Hence, duplicate the fds here, and keep a
     * copy for epoll which we control after use. */

    int fd = fcntl(s, F_DUPFD_CLOEXEC, 3);
    if (fd < 0) {
      return -1;
    }

    if (sd_event_add_io(aur->event_, &io, fd, events, &Aur::OnCurlIO, aur) <
        0) {
      return -1;
    }

    aur->active_io_.emplace(s, io);
    aur->translate_fds_.emplace(fd, s);
  }

  return 0;
}

// static
int Aur::OnCurlIO(sd_event_source*, int fd, uint32_t revents, void* userdata) {
  auto aur = static_cast<Aur*>(userdata);
  int action, k = 0;

  // Throwing an exception here would indicate a bug in Aur::SocketCallback.
  auto translated_fd = aur->translate_fds_[fd];

  if ((revents & (EPOLLIN | EPOLLOUT)) == (EPOLLIN | EPOLLOUT)) {
    action = CURL_POLL_INOUT;
  } else if (revents & EPOLLIN) {
    action = CURL_POLL_IN;
  } else if (revents & EPOLLOUT) {
    action = CURL_POLL_OUT;
  } else {
    action = 0;
  }

  if (curl_multi_socket_action(aur->curl_, translated_fd, action, &k) !=
      CURLM_OK) {
    return -EINVAL;
  }

  return aur->CheckFinished();
}

// static
int Aur::OnCurlTimer(sd_event_source*, uint64_t, void* userdata) {
  auto aur = static_cast<Aur*>(userdata);
  int k = 0;

  if (curl_multi_socket_action(aur->curl_, CURL_SOCKET_TIMEOUT, 0, &k) !=
      CURLM_OK) {
    return -EINVAL;
  }

  return aur->CheckFinished();
}

// static
int Aur::TimerCallback(CURLM*, long timeout_ms, void* userdata) {
  auto aur = static_cast<Aur*>(userdata);

  if (timeout_ms < 0) {
    if (aur->timer_ != nullptr) {
      if (sd_event_source_set_enabled(aur->timer_, SD_EVENT_OFF) < 0) {
        return -1;
      }
    }

    return 0;
  }

  auto usec = TimepointTo<std::chrono::microseconds>(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms));

  if (aur->timer_ != nullptr) {
    if (sd_event_source_set_time(aur->timer_, usec) < 0) {
      return -1;
    }

    if (sd_event_source_set_enabled(aur->timer_, SD_EVENT_ONESHOT) < 0) {
      return -1;
    }
  } else {
    if (sd_event_add_time(aur->event_, &aur->timer_, CLOCK_MONOTONIC, usec, 0,
                          &Aur::OnCurlTimer, aur) < 0) {
      return -1;
    }
  }

  return 0;
}

void Aur::StartRequest(CURL* curl) {
  curl_multi_add_handle(curl_, curl);
  active_requests_.emplace(curl);
}

int Aur::FinishRequest(CURL* curl, CURLcode result, bool dispatch_callback) {
  ResponseHandler* handler;
  curl_easy_getinfo(curl, CURLINFO_PRIVATE, &handler);

  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  std::string error;
  if (result == CURLE_OK) {
    if (response_code != 200) {
      error = "HTTP " + std::to_string(response_code);
    }
  } else {
    const std::string_view buf(handler->error_buffer);
    error = !buf.empty() ? std::string(buf) : curl_easy_strerror(result);
  }

  auto r = dispatch_callback ? handler->RunCallback(error) : 0;

  active_requests_.erase(curl);
  curl_multi_remove_handle(curl_, curl);
  curl_easy_cleanup(curl);

  return r;
}

int Aur::FinishRequest(sd_event_source* source) {
  active_requests_.erase(source);
  sd_event_source_unref(source);
  return 0;
}

int Aur::CheckFinished() {
  int unused;
  auto msg = curl_multi_info_read(curl_, &unused);
  if (msg == nullptr || msg->msg != CURLMSG_DONE) {
    return 0;
  }

  auto r = FinishRequest(msg->easy_handle, msg->data.result,
                         /* dispatch_callback = */ true);
  if (r < 0) {
    CancelAll();
    return r;
  }

  return 0;
}

int Aur::Wait() {
  while (!active_requests_.empty()) {
    if (sd_event_run(event_, 0) < 0) {
      break;
    }
  }

  int r = 0;
  sd_event_get_exit_code(event_, &r);

  return -r;
}

struct RpcRequestTraits {
  using ResponseHandlerType = RpcResponseHandler;

  static constexpr char const* kEncoding = "";
};

struct RawRpcRequestTraits {
  using ResponseHandlerType = RawResponseHandler;

  static constexpr char const* kEncoding = "";
};

struct TarballRequestTraits {
  using ResponseHandlerType = RawResponseHandler;

  static constexpr char const* kEncoding = "identity";
};

struct PkgbuildRequestTraits {
  using ResponseHandlerType = RawResponseHandler;

  static constexpr char const* kEncoding = "";
};

template <typename RequestTraits>
void Aur::QueueRequest(
    const Request& request,
    const typename RequestTraits::ResponseHandlerType::CallbackType& callback) {
  for (const auto& r : request.Build(baseurl_)) {
    auto curl = curl_easy_init();

    auto response_handler =
        new typename RequestTraits::ResponseHandlerType(this, callback);

    using RH = ResponseHandler;
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
    curl_easy_setopt(curl, CURLOPT_URL, r.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &RH::BodyCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_handler);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, response_handler);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, response_handler->error_buffer);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, RequestTraits::kEncoding);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Auracle/0");

    switch (debug_level_) {
      case DEBUG_NONE:
        break;
      case DEBUG_REQUESTS:
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, &RH::DebugCallback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &debug_stream_);
        [[fallthrough]];
      case DEBUG_VERBOSE_STDERR:
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        break;
    }

    StartRequest(curl);
  }
}

// static
int Aur::OnCloneExit(sd_event_source* source, const siginfo_t* si,
                     void* userdata) {
  auto handler = static_cast<CloneResponseHandler*>(userdata);

  handler->aur()->FinishRequest(source);

  std::string error;
  if (si->si_status != 0) {
    error.assign("git exited with unexpected exit status " +
                 std::to_string(si->si_status));
  }

  return handler->RunCallback(error);
}

void Aur::QueueCloneRequest(const CloneRequest& request,
                            const CloneResponseCallback& callback) {
  auto response_handler = new CloneResponseHandler(this, callback);

  const bool update = fs::exists(fs::path(request.reponame()) / ".git");
  if (update) {
    response_handler->SetOperation("update");
  } else {
    response_handler->SetOperation("clone");
  }

  int pid = fork();
  if (pid < 0) {
    response_handler->RunCallback("failed to fork new process for git: " +
                                  std::string(strerror(errno)));
    return;
  }

  if (pid == 0) {
    const auto url = request.Build(baseurl_)[0];

    std::vector<const char*> cmd{"git"};
    if (update) {
      cmd.insert(cmd.end(), {"-C", request.reponame().c_str(), "pull",
                             "--quiet", "--ff-only"});
    } else {
      cmd.insert(cmd.end(), {"clone", "--quiet", url.c_str()});
    }
    cmd.push_back(nullptr);

    execvp(cmd[0], const_cast<char* const*>(cmd.data()));
    _exit(127);
  }

  sd_event_source* child;
  sd_event_add_child(event_, &child, pid, WEXITED, &Aur::OnCloneExit,
                     response_handler);

  active_requests_.emplace(child);
}

void Aur::QueueRawRpcRequest(const RpcRequest& request,
                             const RawResponseCallback& callback) {
  QueueRequest<RawRpcRequestTraits>(request, callback);
}

void Aur::QueueRpcRequest(const RpcRequest& request,
                          const RpcResponseCallback& callback) {
  QueueRequest<RpcRequestTraits>(request, callback);
}

void Aur::QueueTarballRequest(const RawRequest& request,
                              const RawResponseCallback& callback) {
  QueueRequest<TarballRequestTraits>(request, callback);
}

void Aur::QueuePkgbuildRequest(const RawRequest& request,
                               const RawResponseCallback& callback) {
  QueueRequest<PkgbuildRequestTraits>(request, callback);
}

void Aur::SetConnectTimeout(long timeout) { connect_timeout_ = timeout; }

void Aur::SetMaxConnections(long connections) {
  curl_multi_setopt(curl_, CURLMOPT_MAX_TOTAL_CONNECTIONS, connections);
}

}  // namespace aur

/* vim: set et ts=2 sw=2: */
