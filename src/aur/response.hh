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

  RpcResponse() = default;
  RpcResponse(std::vector<Package> packages) : packages(std::move(packages)) {}

  RpcResponse(const RpcResponse&) = default;
  RpcResponse& operator=(const RpcResponse&) = default;

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

#if 0
template <>
struct glz::meta<aur::RpcResponse> {
  using T = aur::RpcResponse;
  static constexpr std::string_view name = "RpcResponse";

  static constexpr auto value = object(  //
      "results", &T::packages,           //
      "resultcount", glz::skip{},        //
      "version", glz::skip{},            //
      "type", glz::skip{});
};
#endif

#endif  // AUR_RESPONSE_HH_
