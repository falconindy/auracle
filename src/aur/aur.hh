// SPDX-License-Identifier: MIT
#ifndef AUR_AUR_HH_
#define AUR_AUR_HH_

#include <functional>
#include <memory>
#include <string>

#include "request.hh"
#include "response.hh"

namespace aur {

class Aur {
 public:
  template <typename ResponseType>
  using ResponseCallback = std::function<int(ResponseWrapper<ResponseType>)>;

  using RpcResponseCallback = ResponseCallback<RpcResponse>;
  using RawResponseCallback = ResponseCallback<RawResponse>;
  using CloneResponseCallback = ResponseCallback<CloneResponse>;

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

  // Clone a git repository.
  virtual void QueueCloneRequest(const CloneRequest& request,
                                 const CloneResponseCallback& callback) = 0;

  // Wait for all pending requests to complete. Returns non-zero if any request
  // failed or was cancelled by a callback.
  virtual int Wait() = 0;
};

std::unique_ptr<Aur> NewAur(Aur::Options options = Aur::Options());

}  // namespace aur

#endif  // AUR_AUR_HH_
