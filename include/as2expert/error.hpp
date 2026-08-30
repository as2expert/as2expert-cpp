// AS2Expert C++ client — typed error hierarchy.
#pragma once

#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace as2expert {

/// Base class for every error raised by the client.
///
/// Carries the HTTP status (when the request reached the server), the parsed
/// error payload, and an optional application error code.
class ApiError : public std::runtime_error {
public:
    ApiError(const std::string& message,
             std::optional<int> status = std::nullopt,
             nlohmann::json payload = nlohmann::json(),
             std::optional<std::string> code = std::nullopt)
        : std::runtime_error(message),
          status_(status),
          code_(std::move(code)),
          payload_(std::move(payload)) {}

    /// HTTP status code, or std::nullopt for transport-level failures.
    const std::optional<int>& status() const noexcept { return status_; }
    /// Application error code from the payload, when present.
    const std::optional<std::string>& code() const noexcept { return code_; }
    /// Raw error payload (the JSON body), when it parsed.
    const nlohmann::json& payload() const noexcept { return payload_; }

private:
    std::optional<int> status_;
    std::optional<std::string> code_;
    nlohmann::json payload_;
};

/// 401 / 403 — missing/invalid token or insufficient scope.
class AuthError : public ApiError {
    using ApiError::ApiError;
};

/// 400 / 422 — the request failed server-side validation.
class ValidationError : public ApiError {
public:
    ValidationError(const std::string& message, std::optional<int> status,
                    nlohmann::json payload, std::optional<std::string> code,
                    nlohmann::json fields)
        : ApiError(message, status, std::move(payload), std::move(code)),
          fields_(std::move(fields)) {}

    /// Field-level validation errors (a JSON array), when the API returns them.
    const nlohmann::json& fields() const noexcept { return fields_; }

private:
    nlohmann::json fields_;
};

/// 404 — the resource does not exist.
class NotFoundError : public ApiError {
    using ApiError::ApiError;
};

/// 429 — too many requests.
class RateLimitError : public ApiError {
public:
    RateLimitError(const std::string& message, std::optional<int> status,
                   nlohmann::json payload, std::optional<std::string> code,
                   std::optional<double> retry_after)
        : ApiError(message, status, std::move(payload), std::move(code)),
          retry_after_(retry_after) {}

    /// Seconds to wait before retrying, when the API provides it.
    const std::optional<double>& retry_after() const noexcept { return retry_after_; }

private:
    std::optional<double> retry_after_;
};

/// 5xx — the API failed.
class ServerError : public ApiError {
    using ApiError::ApiError;
};

/// The request never completed (connection / timeout / TLS / decode).
class TransportError : public ApiError {
    using ApiError::ApiError;
};

}  // namespace as2expert
