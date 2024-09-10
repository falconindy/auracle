// SPDX-License-Identifier: MIT
#ifndef AUR_RESPONSE_HH_
#define AUR_RESPONSE_HH_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "aur/package.hh"

namespace aur {

struct CloneResponse {
  static absl::StatusOr<CloneResponse> Parse(std::string_view operation) {
    return CloneResponse(operation);
  }

  explicit CloneResponse(std::string_view operation) : operation(operation) {}

  CloneResponse(const CloneResponse&) = delete;
  CloneResponse& operator=(const CloneResponse&) = delete;

  CloneResponse(CloneResponse&&) = default;
  CloneResponse& operator=(CloneResponse&&) = default;

  std::string_view operation;
};

struct RpcResponse {
  static absl::StatusOr<RpcResponse> Parse(std::string_view bytes);

  RpcResponse(std::vector<Package> packages) : packages(std::move(packages)) {}

  RpcResponse(const RpcResponse&) = delete;
  RpcResponse& operator=(const RpcResponse&) = delete;

  RpcResponse(RpcResponse&&) = default;
  RpcResponse& operator=(RpcResponse&&) = default;

  std::vector<Package> packages;
};

struct RawResponse {
  static absl::StatusOr<RawResponse> Parse(std::string bytes) {
    return RawResponse(std::move(bytes));
  }

  RawResponse(std::string bytes) : bytes(std::move(bytes)) {}

  RawResponse(const RawResponse&) = delete;
  RawResponse& operator=(const RawResponse&) = delete;

  RawResponse(RawResponse&&) = default;
  RawResponse& operator=(RawResponse&&) = default;

  std::string bytes;
};

}  // namespace aur

#endif  // AUR_RESPONSE_HH_
