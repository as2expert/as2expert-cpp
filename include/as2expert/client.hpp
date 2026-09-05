// AS2Expert C++ client — public API.
//
// Idiomatic RAII: construct a Client, use resource accessors (client.messages(),
// client.edifact(), ...), and let destruction release the underlying HTTP
// resources. Methods return nlohmann::json (list methods return a JSON array)
// and throw a subclass of as2expert::ApiError on failure.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "as2expert/error.hpp"

namespace as2expert {

using json = nlohmann::json;

/// Resolve a named environment ("free" / "b2b") to its base URL. Returns an
/// empty string for unknown names.
std::string environment_url(const std::string& name);

/// Client configuration. Set either `base_url` or `environment`.
struct Config {
    std::string token;
    std::string base_url;                      ///< e.g. https://free.as2expert.com/api/v1
    std::string environment;                   ///< "free" | "b2b" (used if base_url empty)
    std::chrono::milliseconds timeout{std::chrono::seconds(30)};
    int max_retries = 2;                       ///< retries on 429 / 5xx / transient
    bool verify_tls = true;
    std::string user_agent;                    ///< empty → library default
};

class Transport;  // internal

/// Message operations.
class Messages {
public:
    explicit Messages(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    json list(const json& filter = json::object());
    /// List a station's folders (id, name, parent_id, count, icono, …).
    json folders(const json& filter = json::object());
    json get(const json& id);
    /// Download the payload as raw bytes (base64-decoded).
    std::vector<std::uint8_t> download(const json& id);
    /// Send a file to a partner; `content` is base64-encoded for you.
    json send(const json& partner, const std::string& subject,
              const std::string& file_name, const std::vector<std::uint8_t>& content);
    json send(const json& partner, const std::string& subject,
              const std::string& file_name, const std::string& content);
    json mark_read(const json& id);
    json mark_unread(const json& id);
    json move_to(const json& id, const json& folder);
    json remove(const json& id);
    json changes(const json& params = json::object());

private:
    std::shared_ptr<Transport> t_;
};

class Partners {
public:
    explicit Partners(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    json list(const json& filter = json::object());
    json get(const json& id);
    json create(const json& partner);
    /// Update a partner's identity fields; `partner` must carry `id`.
    json update(const json& partner);
    json remove(const json& id);

private:
    std::shared_ptr<Transport> t_;
};

class Certificates {
public:
    explicit Certificates(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    json list();
    json get(const json& id);
    json create(const json& cert);

private:
    std::shared_ptr<Transport> t_;
};

class Stations {
public:
    explicit Stations(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    json list(const json& filter = json::object());
    json get(const json& id);
    json stats(const json& id);
    json create(const json& station);
    /// Update a station's identity fields; `station` must carry `id`.
    json update(const json& station);
    json remove(const json& id);

private:
    std::shared_ptr<Transport> t_;
};

class Webhooks {
public:
    explicit Webhooks(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    json configure(const json& config);
    json get();
    json test();
    json logs(const json& params = json::object());

private:
    std::shared_ptr<Transport> t_;
};

class BusinessDocuments {
public:
    explicit BusinessDocuments(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    /// Create a business document. Pass an idempotency key to make retries safe.
    json create(const json& document, const std::string& idempotency_key = "");
    json get(const json& business_document_id);
    json diagnostics(const json& params = json::object());

private:
    std::shared_ptr<Transport> t_;
};

class Edifact {
public:
    explicit Edifact(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    json analyze(const std::string& edifact);
    json validate(const std::string& edifact);  ///< alias of analyze
    /// Translate an interchange to "json" / "xml" / "text".
    json convert(const std::string& edifact, const std::string& format = "json");
    /// Build a functional acknowledgement (kind: "contrl" or "aperak").
    json acknowledge(const std::string& edifact, const std::string& kind = "contrl",
                     bool acknowledged = true, const json& errors = json::array());
    json skeleton(const std::string& message_type, const std::string& release,
                  bool compose = false);

private:
    std::shared_ptr<Transport> t_;
};

class Dashboard {
public:
    explicit Dashboard(std::shared_ptr<Transport> t) : t_(std::move(t)) {}
    json kpis();

private:
    std::shared_ptr<Transport> t_;
};

/// The AS2Expert API client.
///
/// Example:
/// @code
///   as2expert::Config cfg;
///   cfg.token = "...";
///   cfg.environment = "free";
///   as2expert::Client client(cfg);
///   auto out = client.edifact().convert("UNB+...'", "json");
///   std::cout << out["filename"] << "\n";
/// @endcode
class Client {
public:
    explicit Client(Config config);
    ~Client();
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    const std::string& base_url() const noexcept;

    Messages& messages() noexcept { return messages_; }
    Partners& partners() noexcept { return partners_; }
    Certificates& certificates() noexcept { return certificates_; }
    Stations& stations() noexcept { return stations_; }
    Webhooks& webhooks() noexcept { return webhooks_; }
    BusinessDocuments& business_documents() noexcept { return business_documents_; }
    Edifact& edifact() noexcept { return edifact_; }
    Dashboard& dashboard() noexcept { return dashboard_; }

private:
    std::shared_ptr<Transport> t_;
    Messages messages_;
    Partners partners_;
    Certificates certificates_;
    Stations stations_;
    Webhooks webhooks_;
    BusinessDocuments business_documents_;
    Edifact edifact_;
    Dashboard dashboard_;
};

// ---- Webhook signature verification -----------------------------------------

/// Compute the "sha256=<hex>" signature for a timestamp + body.
std::string sign_payload(const std::string& secret, const std::string& timestamp,
                         const std::string& body);

/// Verify an AS2Expert webhook signature and freshness.
///
/// AS2Expert signs deliveries with HMAC-SHA256 over "<timestamp>.<body>", sent in
/// X-AS2Expert-Timestamp and X-AS2Expert-Signature ("sha256=<hex>"). `now` is the
/// current unix time in seconds; the check fails if the timestamp is more than
/// `tolerance_secs` away.
bool verify_signature(const std::string& secret, const std::string& timestamp,
                      const std::string& body, const std::string& signature,
                      long tolerance_secs, long now);

/// Default signature tolerance window, in seconds (5 minutes).
constexpr long kDefaultToleranceSecs = 300;

}  // namespace as2expert
