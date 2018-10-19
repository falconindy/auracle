#include "response.hh"

#include "json_internal.hh"

namespace aur {

void from_json(const nlohmann::json& j, RpcResponse& r) {
  for (auto iter = j.begin(); iter != j.end(); iter++) {
    if (iter.key() == "type") {
      from_json(iter.value(), r.type);
    } else if (iter.key() == "error") {
      from_json(iter.value(), r.error);
    } else if (iter.key() == "resultcount") {
      from_json(iter.value(), r.resultcount);
    } else if (iter.key() == "version") {
      from_json(iter.value(), r.version);
    } else if (iter.key() == "results") {
      from_json(iter.value(), r.results);
    }
  }
}

// static
RpcResponse RpcResponse::Parse(const std::string& json_bytes) {
  return nlohmann::json::parse(json_bytes);
}

}  // namespace aur
