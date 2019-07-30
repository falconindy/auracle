#ifndef RESPONSE_HH
#define RESPONSE_HH

#include "package.hh"

namespace aur {

template <typename ResponseT>
class ResponseWrapper {
 public:
  ResponseWrapper(ResponseT value, long status, std::string error)
      : value_(std::move(value)), status_(status), error_(std::move(error)) {}

  ResponseWrapper(const ResponseWrapper&) = delete;
  ResponseWrapper& operator=(const ResponseWrapper&) = delete;

  ResponseWrapper(ResponseWrapper&&) noexcept = default;
  ResponseWrapper& operator=(ResponseWrapper&&) noexcept = default;

  const ResponseT& value() const { return value_; }
  ResponseT&& value() { return std::move(value_); }

  const std::string& error() const { return error_; }
  long status() const { return status_; }

  bool ok() const { return error_.empty(); }

 private:
  ResponseT value_;
  long status_;
  std::string error_;
};

struct CloneResponse {
  explicit CloneResponse(std::string operation)
      : operation(std::move(operation)) {}

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
