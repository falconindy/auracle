#ifndef JSON_INTERNAL_HH
#define JSON_INTERNAL_HH

#include "nlohmann/json.hpp"

#include "package.hh"

namespace aur {

void from_json(const nlohmann::json& j, Package& p);

template <typename T>
void from_json(const nlohmann::json& j, T& s) {
  if (j.is_null()) {
    return;
  }
  s = j.get<T>();
}

template <typename T>
void from_json(const nlohmann::json& j, std::vector<T>& vs) {
  if (j.is_null()) {
    return;
  }

  vs.reserve(j.size());
  for (auto iter = j.begin(); iter != j.end(); iter++) {
    vs.emplace_back(iter->get<T>());
  }
}

}  // namespace aur

#endif  // JSON_INTERNAL_HH
