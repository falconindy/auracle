#include "response.hh"

#include "json_internal.hh"

namespace aur {

void from_json(const nlohmann::json& j, RpcResponse& r) {
  for (const auto& iter : j.items()) {
    const auto& key = iter.key();
    const auto& value = iter.value();

    if (key == "type") {
      from_json(value, r.type);
    } else if (key == "error") {
      from_json(value, r.error);
    } else if (key == "resultcount") {
      from_json(value, r.resultcount);
    } else if (key == "version") {
      from_json(value, r.version);
    } else if (key == "results") {
      from_json(value, r.results);
    }
  }
}

// static
RpcResponse::RpcResponse(const std::string& json_bytes) {
  *this = nlohmann::json::parse(json_bytes);
}

}  // namespace aur
