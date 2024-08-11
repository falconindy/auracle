// SPDX-License-Identifier: MIT
#ifndef AUR_RESPONSE_HH_
#define AUR_RESPONSE_HH_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "aur/package.hh"

namespace aur {

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
