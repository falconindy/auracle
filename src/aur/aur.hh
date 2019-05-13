#ifndef AUR_HH
#define AUR_HH

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <curl/curl.h>
#include <systemd/sd-event.h>

#include "request.hh"
#include "response.hh"

namespace aur {

class Aur {
 public:
  using RpcResponseCallback = std::function<int(ResponseWrapper<RpcResponse>)>;
  using RawResponseCallback = std::function<int(ResponseWrapper<RawResponse>)>;
  using CloneResponseCallback =
      std::function<int(ResponseWrapper<CloneResponse>)>;

  struct Options {
    Options() = default;

    Options(const Options&) = default;
    Options& operator=(const Options&) = default;

    Options(Options&&) = default;
    Options& operator=(Options&&) = default;

    Options& set_baseurl(std::string baseurl) {
      this->baseurl = std::move(baseurl);
      return *this;
    }
    std::string baseurl;

    Options& set_useragent(std::string useragent) {
      this->useragent = std::move(useragent);
      return *this;
    }
    std::string useragent;
  };

  // Construct a new Aur object, rooted at the given URL, e.g.
  // https://aur.archlinux.org.
  explicit Aur(Options options = Options());
  ~Aur();

  Aur(const Aur&) = delete;
  Aur& operator=(const Aur&) = delete;

  Aur(Aur&&) = default;
  Aur& operator=(Aur&&) = default;

  // Asynchronously issue an RPC request. The callback will be invoked when the
  // call completes.
  void QueueRpcRequest(const RpcRequest& request,
                       const RpcResponseCallback& callback);

  // Asynchronously issue a raw request. The callback will be invoked when the
  // call completes.
  void QueueRawRequest(const HttpRequest& request,
                       const RawResponseCallback& callback);

  // Asynchronously issue a download request. The callback will be invoked when
  // the call completes.
  void QueueTarballRequest(const RawRequest& request,
                           const RawResponseCallback& callback);

  // Clone a git repository.
  void QueueCloneRequest(const CloneRequest& request,
                         const CloneResponseCallback& callback);

  // Wait for all pending requests to complete. Returns non-zero if any request
  // failed or was cancelled by a callback.
  int Wait();

 private:
  using ActiveRequests =
      std::unordered_set<std::variant<CURL*, sd_event_source*>>;

  template <typename RequestType>
  void QueueHttpRequest(
      const HttpRequest& request,
      const typename RequestType::ResponseHandlerType::CallbackType& callback);

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
                            void* userdata, void* socketp);
  static int TimerCallback(CURLM* curl, long timeout_ms, void* userdata);
  static int OnCurlIO(sd_event_source* s, int fd, uint32_t revents,
                      void* userdata);
  static int OnCurlTimer(sd_event_source* s, uint64_t usec, void* userdata);
  static int OnCloneExit(sd_event_source* s, const siginfo_t* si,
                         void* userdata);

  Options options_;

  CURLM* curl_multi_;
  ActiveRequests active_requests_;
  std::unordered_map<int, sd_event_source*> active_io_;
  std::unordered_map<int, int> translate_fds_;

  sigset_t saved_ss_{};
  sd_event* event_ = nullptr;
  sd_event_source* timer_ = nullptr;

  DebugLevel debug_level_ = DebugLevel::NONE;
  std::ofstream debug_stream_;
};

}  // namespace aur

#endif  // AUR_HH
