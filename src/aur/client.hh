// SPDX-License-Identifier: MIT
#ifndef AUR_CLIENT_HH_
#define AUR_CLIENT_HH_

#include <functional>
#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "aur/request.hh"
#include "aur/response.hh"

namespace aur {

class Client {
 public:
  template <typename ResponseType>
  using ResponseCallback =
      absl::AnyInvocable<int(absl::StatusOr<ResponseType>) &&>;

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

  static std::unique_ptr<Client> New(Client::Options options = {});

  Client() = default;
  virtual ~Client() = default;

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  Client(Client&&) = default;
  Client& operator=(Client&&) = default;

  // Asynchronously issue an RPC request using the REST API. The callback will
  // be invoked when the call completes.
  virtual void QueueRpcRequest(const RpcRequest& request,
                               RpcResponseCallback callback) = 0;

  // Asynchronously issue a raw request. The callback will be invoked when the
  // call completes.
  virtual void QueueRawRequest(const HttpRequest& request,
                               RawResponseCallback callback) = 0;

  // Clone a git repository.
  virtual void QueueCloneRequest(const CloneRequest& request,
                                 CloneResponseCallback callback) = 0;

  // Wait for all pending requests to complete. Returns non-zero if any request
  // failed or was cancelled by a callback.
  virtual int Wait() = 0;
};

}  // namespace aur

#endif  // AUR_AUR_HH_
