#ifndef JSON_INTERNAL_HH
#define JSON_INTERNAL_HH

#include "nlohmann/json.hpp"

#include "package.hh"

namespace aur {

// NB: We only really need the Package overload of from_json here. All known
// overloads of from_json are listed anyways for consistency.
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
