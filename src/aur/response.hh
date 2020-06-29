// SPDX-License-Identifier: MIT
#ifndef AUR_RESPONSE_HH_
#define AUR_RESPONSE_HH_

#include "absl/status/status.h"
#include "package.hh"

namespace aur {

template <typename ResponseT>
class ResponseWrapper {
 public:
  ResponseWrapper(ResponseT value, absl::Status status)
      : value_(std::move(value)), status_(std::move(status)) {}

  ResponseWrapper(const ResponseWrapper&) = delete;
  ResponseWrapper& operator=(const ResponseWrapper&) = delete;

  ResponseWrapper(ResponseWrapper&&) noexcept = default;
  ResponseWrapper& operator=(ResponseWrapper&&) noexcept = default;

  const ResponseT& value() const { return value_; }
  ResponseT&& value() { return std::move(value_); }

  const absl::Status& status() const { return status_; }
  bool ok() const { return status_.ok(); }

 private:
  ResponseT value_;
  absl::Status status_;
};

struct CloneResponse {
  explicit CloneResponse(std::string operation)
      : operation(std::move(operation)) {}

  CloneResponse(const CloneResponse&) = delete;
  CloneResponse& operator=(const CloneResponse&) = delete;

  CloneResponse(CloneResponse&&) = default;
  CloneResponse& operator=(CloneResponse&&) = default;

  std::string operation;
};

struct RpcResponse {
  RpcResponse() = default;
  explicit RpcResponse(const std::string& json_bytes);

  RpcResponse(const RpcResponse&) = delete;
  RpcResponse& operator=(const RpcResponse&) = delete;

  RpcResponse(RpcResponse&&) = default;
  RpcResponse& operator=(RpcResponse&&) = default;

  std::string type;
  std::string error;
  int resultcount;
  std::vector<Package> results;
  int version;
};

struct RawResponse {
  RawResponse(std::string bytes) : bytes(std::move(bytes)) {}

  RawResponse(const RawResponse&) = delete;
  RawResponse& operator=(const RawResponse&) = delete;

  RawResponse(RawResponse&&) = default;
  RawResponse& operator=(RawResponse&&) = default;

  std::string bytes;
};

}  // namespace aur

#endif  // AUR_RESPONSE_HH_
