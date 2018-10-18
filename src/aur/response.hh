#ifndef RESPONSE_HH
#define RESPONSE_HH

#include "package.hh"

#include <variant>

namespace aur {

template <typename T>
class StatusOr : public std::variant<std::string, T> {
 public:
  using std::variant<std::string, T>::variant;

  bool ok() const { return std::holds_alternative<T>(*this); }

  const std::string& error() const { return std::get<std::string>(*this); }

  const T& value() const { return std::get<T>(*this); }
  T&& value() { return std::move(std::get<T>(*this)); }
};

struct CloneResponse {
  std::string operation;
};

struct RpcResponse {
  std::string type;
  std::string error;
  int resultcount;
  std::vector<Package> results;
  int version;

  static RpcResponse Parse(const std::string& json_bytes);
};

struct RawResponse {
  std::string filename_hint;
  std::string bytes;
};

}  // namespace aur

#endif  // RESPONSE_HH
