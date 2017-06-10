#ifndef RESPONSE_HH
#define RESPONSE_HH

#include "package.hh"

#include <variant>

namespace aur {

template <typename Value>
class HttpStatusOr : public std::variant<std::string, Value> {
 public:
  using std::variant<std::string, Value>::variant;

  bool ok() const { return std::get_if<std::string>(this) == nullptr; }

  const std::string& error() const { return std::get<std::string>(*this); }

  const Value& value() const { return std::get<Value>(*this); }
};

struct RpcResponse {
  std::string type;
  std::string error;
  int resultcount;
  std::vector<Package> results;
  int version;

  static RpcResponse Parse(const std::string& json_bytes);
};

struct DownloadResponse {
  std::string filename;
  std::string archive_bytes;
};

}  // namespace aur

#endif  // RESPONSE_HH
