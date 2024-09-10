// SPDX-License-Identifier: MIT
#include "aur/response.hh"

#include "aur/json_internal.hh"

using nlohmann::json;

namespace aur {

absl::StatusOr<RpcResponse> RpcResponse::Parse(std::string_view bytes) {
  try {
    const json j = json::parse(bytes);
    if (const auto iter = j.find("error"); iter != j.end()) {
      return absl::InvalidArgumentError(iter->get<std::string_view>());
    }

    return RpcResponse(j["results"].get<std::vector<Package>>());
  } catch (const std::exception& e) {
    return absl::UnknownError(e.what());
  }
}

}  // namespace aur
