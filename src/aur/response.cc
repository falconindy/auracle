#include "response.hh"

#include "json_internal.hh"

namespace aur {
namespace fields {

DEFINE_FIELD(type, std::string, RpcResponse::type);
DEFINE_FIELD(error, std::string, RpcResponse::error);
DEFINE_FIELD(resultcount, int, RpcResponse::resultcount);
DEFINE_FIELD(version, int, RpcResponse::version);
DEFINE_FIELD(results, std::vector<Package>, RpcResponse::results);

}  // namespace fields

void from_json(const nlohmann::json& j, RpcResponse& r) {
  Store<fields::type>(j, r);
  Store<fields::error>(j, r);
  Store<fields::resultcount>(j, r);
  Store<fields::version>(j, r);
  Store<fields::results>(j, r);
}

// static
RpcResponse RpcResponse::Parse(const std::string& json_bytes) {
  return RpcResponse(nlohmann::json::parse(json_bytes));
}

}  // namespace aur
