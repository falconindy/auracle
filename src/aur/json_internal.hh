#ifndef JSON_INTERNAL_HH
#define JSON_INTERNAL_HH

#include "json.hpp"

#include "package.hh"
#include "response.hh"

namespace aur {

// NB: We only really need the Package overload of from_json here. All known
// overloads of from_json are listed anyways for consistency.
void from_json(const nlohmann::json& j, Package& p);
void from_json(const nlohmann::json& j, Dependency& p);
void from_json(const nlohmann::json& j, RpcResponse& p);

template <typename FieldT, typename ParsedType>
void Store(const nlohmann::json& j, ParsedType& r) {
  auto iter = j.find(FieldT::kFieldName);
  if (iter == j.end() || iter->is_null()) {
    return;
  }

  (r.*FieldT::Dest) = iter.value().template get<typename FieldT::FieldType>();
}

}  // namespace aur

#define DEFINE_FIELD(JsonField, CppType, StructField) \
  struct JsonField {                                  \
    using FieldType = CppType;                        \
    static constexpr char kFieldName[] = #JsonField;  \
    static constexpr auto Dest = &StructField;        \
  }

#endif  // JSON_INTERNAL_HH
