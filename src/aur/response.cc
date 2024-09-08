// SPDX-License-Identifier: MIT
#include "aur/response.hh"

#include "absl/base/no_destructor.h"
#include "aur/json_internal.hh"

using nlohmann::json;

namespace aur {

absl::StatusOr<RpcResponse> RpcResponse::Parse(std::string_view bytes) {
  try {
    json j = json::parse(bytes);
    if (auto iter = j.find("error"); iter != j.end()) {
      return absl::InvalidArgumentError(iter->get<std::string>());
    }

    return RpcResponse(j["results"].get<std::vector<Package>>());
  } catch (const std::exception& e) {
    return absl::UnknownError(e.what());
  }
}

}  // namespace aur
