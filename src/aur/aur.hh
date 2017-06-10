#ifndef AUR_HH
#define AUR_HH

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <curl/curl.h>

#include "request.hh"
#include "response.hh"

namespace aur {

class Aur {
 public:
  using RpcResponseCallback = std::function<int(HttpStatusOr<RpcResponse>)>;
  using DownloadResponseCallback =
      std::function<int(HttpStatusOr<DownloadResponse>)>;

  // Construct a new Aur object, rooted at the given URL, e.g.
  // https://aur.archlinux.org.
  explicit Aur(const std::string& baseurl);
  ~Aur();

  Aur(const Aur&) = delete;
  Aur& operator=(const Aur&) = delete;
  Aur(Aur&&) = default;

  // Sets the maximum number of allowed simultaneous connections open to the AUR
  // server at any given time. Set to 0 to specify unlimited connections.
  void SetMaxConnections(long connections);

  // Sets the connection timeout, in seconds for each connection to the AUR
  // server. Set to 0 to specify no timeout.
  void SetConnectTimeout(long timeout);

  // Asynchronously issue an RPC request. The callback will be invoked when the
  // call completes.
  void QueueRpcRequest(const RpcRequest* request,
                       const RpcResponseCallback& callback);

  // Asynchronously issue a download request. The callback will be invoked when
  // the call completes.
  void QueueDownloadRequest(const DownloadRequest* request,
                            const DownloadResponseCallback& callback);

  // Wait for all pending requests to complete. Returns non-zero if any request
  // failed or was cancelled by a callback.
  int Wait();

 private:
  template <typename RequestType>
  void QueueRequest(const Request* request,
                    const typename RequestType::CallbackType& callback);

  void StartRequest(CURL* curl);
  int FinishRequest(CURL* curl, CURLcode result, bool dispatch_callback);

  int ProcessDoneEvents();
  void Cancel();

  const std::string baseurl_;

  long connect_timeout_ = 10;

  CURLM* curl_multi_;
  std::unordered_set<CURL*> active_requests_;
};

}  // namespace

#endif  // AUR_HH
