// Internal HTTP transport around libcurl. One POST per API call, retrying 429/5xx.
#pragma once

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "as2expert/client.hpp"

namespace as2expert {

class Transport {
public:
    explicit Transport(Config cfg);

    const std::string& base_url() const noexcept { return base_url_; }

    /// POST `body` to `path`; returns the decoded `data` field, or the whole
    /// payload when there is no `data`. Throws a subclass of ApiError on failure.
    nlohmann::json post(const std::string& path, const nlohmann::json& body);

    /// POST with extra request headers (e.g. Idempotency-Key).
    nlohmann::json post(const std::string& path, const nlohmann::json& body,
                        const std::vector<std::pair<std::string, std::string>>& headers);

private:
    Config cfg_;
    std::string base_url_;
    std::string user_agent_;
};

}  // namespace as2expert
