// SPDX-License-Identifier: MIT
#include "aur/response.hh"

#include "absl/base/no_destructor.h"
#include "aur/json_internal.hh"

namespace aur::internal {

void from_json(const nlohmann::json& j, RawRpcResponse& r) {
  // clang-format off
  static const absl::NoDestructor<CallbackMap<RawRpcResponse>> kCallbacks({
    { "error",        MakeValueCallback(&RawRpcResponse::error) },
    { "results",      MakeValueCallback(&RawRpcResponse::results) },
  });
  // clang-format on

  DeserializeJsonObject(j, *kCallbacks, r);
}

RawRpcResponse::RawRpcResponse(const std::string& json_bytes) {
  if (json_bytes.empty()) {
    return;
  }

  try {
    *this = nlohmann::json::parse(json_bytes);
  } catch (const std::exception& e) {
    error = e.what();
  }
}

}  // namespace aur::internal
