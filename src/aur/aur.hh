#ifndef AUR_HH
#define AUR_HH

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

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

  Aur() = default;
  virtual ~Aur() = default;

  Aur(const Aur&) = delete;
  Aur& operator=(const Aur&) = delete;

  Aur(Aur&&) = default;
  Aur& operator=(Aur&&) = default;

  // Asynchronously issue an RPC request. The callback will be invoked when the
  // call completes.
  virtual void QueueRpcRequest(const RpcRequest& request,
                               const RpcResponseCallback& callback) = 0;

  // Asynchronously issue a raw request. The callback will be invoked when the
  // call completes.
  virtual void QueueRawRequest(const HttpRequest& request,
                               const RawResponseCallback& callback) = 0;

  // Asynchronously issue a download request. The callback will be invoked when
  // the call completes.
  virtual void QueueTarballRequest(const RawRequest& request,
                                   const RawResponseCallback& callback) = 0;

  // Clone a git repository.
  virtual void QueueCloneRequest(const CloneRequest& request,
                                 const CloneResponseCallback& callback) = 0;

  // Wait for all pending requests to complete. Returns non-zero if any request
  // failed or was cancelled by a callback.
  virtual int Wait() = 0;
};

std::unique_ptr<Aur> NewAur(Aur::Options options = Aur::Options());

}  // namespace aur

#endif  // AUR_HH
