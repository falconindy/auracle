// SPDX-License-Identifier: MIT
#include "response.hh"

#include "json_internal.hh"

namespace aur {

void from_json(const nlohmann::json& j, RpcResponse& r) {
  // clang-format off
  static const auto& callbacks = *new CallbackMap<RpcResponse>{
    { "type",         MakeValueCallback(&RpcResponse::type) },
    { "error",        MakeValueCallback(&RpcResponse::error) },
    { "resultcount",  MakeValueCallback(&RpcResponse::resultcount) },
    { "version",      MakeValueCallback(&RpcResponse::version) },
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
    type = "error";
    error = e.what();
  }
}

}  // namespace aur
