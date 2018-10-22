#ifndef JSON_INTERNAL_HH
#define JSON_INTERNAL_HH

#include "nlohmann/json.hpp"

#include "package.hh"

namespace aur {

void from_json(const nlohmann::json& j, Package& p);

template <typename T>
void from_json(const nlohmann::json& j, T& v) {
  if (j.is_null()) {
    return;
  }

  if constexpr (std::is_assignable<T&, decltype(j)>::value) {
    v = j;
  } else {
    v = T(j);
  }
}

template <typename T>
void from_json(const nlohmann::json& j, std::vector<T>& v) {
  if (j.is_null()) {
    return;
  }

  v = std::vector<T>(j.begin(), j.end());
}

}  // namespace aur

#endif  // JSON_INTERNAL_HH
