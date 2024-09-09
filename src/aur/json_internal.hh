// SPDX-License-Identifier: MIT
#ifndef AUR_JSON_INTERNAL_HH_
#define AUR_JSON_INTERNAL_HH_

#include "absl/container/flat_hash_map.h"
#include "aur/package.hh"
#include "nlohmann/json.hpp"

namespace aur {

void from_json(const nlohmann::json& j, Package& p);

template <typename OutputType>
using ValueCallback = std::function<void(const nlohmann::json&, OutputType&)>;

template <typename OutputType, typename FieldType>
ValueCallback<OutputType> MakeValueCallback(FieldType OutputType::*field) {
  return [=](const nlohmann::json& j, OutputType& o) {
    j.get_to<FieldType>(o.*field);
  };
}

template <typename OutputType>
using CallbackMap =
    absl::flat_hash_map<std::string_view, ValueCallback<OutputType>>;

template <typename OutputType>
void DeserializeJsonObject(const nlohmann::json& j,
                           const CallbackMap<OutputType>& callbacks,
                           OutputType& o) {
  for (const auto& [key, value] : j.items()) {
    // Skip all null-valued entries.
    if (value.is_null()) {
      continue;
    }

    if (const auto iter = callbacks.find(key); iter != callbacks.end()) {
      iter->second(value, o);
    }
  }
}

}  // namespace aur

#endif  // AUR_JSON_INTERNAL_HH_
