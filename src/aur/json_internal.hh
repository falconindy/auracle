// SPDX-License-Identifier: MIT
#ifndef AUR_JSON_INTERNAL_HH_
#define AUR_JSON_INTERNAL_HH_

#include "absl/container/flat_hash_map.h"
#include "nlohmann/json.hpp"
#include "package.hh"

namespace aur {

void from_json(const nlohmann::json& j, Package& p);
void from_json(const nlohmann::json& j, absl::Time& t);

template <typename T>
void from_json(const nlohmann::json& j, T& v) {
  if (j.is_null()) {
    return;
  }

  v = T(j);
}

template <typename T>
void from_json(const nlohmann::json& j, std::vector<T>& v) {
  if (j.is_null()) {
    return;
  }

  v = std::vector<T>(j.cbegin(), j.cend());
}

template <typename OutputType>
using ValueCallback = std::function<void(const nlohmann::json&, OutputType&)>;

template <typename OutputType, typename FieldType>
ValueCallback<OutputType> MakeValueCallback(FieldType OutputType::*field) {
  return
      [=](const nlohmann::json& j, OutputType& o) { from_json(j, o.*field); };
}

template <typename OutputType>
using CallbackMap =
    absl::flat_hash_map<std::string_view, ValueCallback<OutputType>>;

template <typename OutputType>
void DeserializeJsonObject(const nlohmann::json& j,
                           const CallbackMap<OutputType>& callbacks,
                           OutputType& o) {
  for (const auto& [key, value] : j.items()) {
    const auto iter = callbacks.find(key);
    if (iter != callbacks.end()) {
      iter->second(value, o);
    }
  }
}

}  // namespace aur

#endif  // AUR_JSON_INTERNAL_HH_
