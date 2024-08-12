// SPDX-License-Identifier: MIT
#include "aur/response.hh"

#include "aur/json_internal.hh"

namespace aur {

void from_json(const nlohmann::json& j, RpcResponse& r) {
  // clang-format off
  static const auto& callbacks = *new CallbackMap<RpcResponse>{
    { "error",        MakeValueCallback(&RpcResponse::error) },
    { "results",      MakeValueCallback(&RpcResponse::results) },
  };
  // clang-format on

  DeserializeJsonObject(j, callbacks, r);
}

// static
RpcResponse::RpcResponse(const std::string& json_bytes) {
  if (json_bytes.empty()) {
    return;
  }

  try {
    *this = nlohmann::json::parse(json_bytes);
  } catch (const std::exception& e) {
    error = e.what();
  }
}

}  // namespace aur
