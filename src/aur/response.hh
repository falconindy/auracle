#ifndef RESPONSE_HH
#define RESPONSE_HH

#include "package.hh"

#include <variant>

namespace aur {

template <typename T>
class StatusOr : public std::variant<std::string, T> {
 public:
  using std::variant<std::string, T>::variant;

  StatusOr(const StatusOr&) = delete;
  StatusOr& operator=(const StatusOr&) = delete;

  StatusOr(StatusOr&&) = default;
  StatusOr& operator=(StatusOr&&) = default;

  bool ok() const { return std::holds_alternative<T>(*this); }

  const std::string& error() const { return std::get<std::string>(*this); }

  const T& value() const { return std::get<T>(*this); }
  T&& value() { return std::move(std::get<T>(*this)); }
};

struct CloneResponse {
  CloneResponse(std::string operation) : operation(std::move(operation)) {}

  CloneResponse(const CloneResponse&) = delete;
  CloneResponse& operator=(const CloneResponse&) = delete;

  CloneResponse(CloneResponse&&) = default;
  CloneResponse& operator=(CloneResponse&&) = default;

  std::string operation;
};

struct RpcResponse {
  RpcResponse() = default;
  explicit RpcResponse(const std::string& json_bytes);

  RpcResponse(const RpcResponse&) = delete;
  RpcResponse& operator=(const RpcResponse&) = delete;

  RpcResponse(RpcResponse&&) = default;
  RpcResponse& operator=(RpcResponse&&) = default;

  std::string type;
  std::string error;
  int resultcount;
  std::vector<Package> results;
  int version;
};

struct RawResponse {
  RawResponse(std::string bytes) : bytes(std::move(bytes)) {}

  RawResponse(const RawResponse&) = delete;
  RawResponse& operator=(const RawResponse&) = delete;

  RawResponse(RawResponse&&) = default;
  RawResponse& operator=(RawResponse&&) = default;

  std::string bytes;
};

}  // namespace aur

#endif  // RESPONSE_HH
