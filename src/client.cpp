#include "as2expert/client.hpp"

#include "base64.hpp"
#include "transport.hpp"

namespace as2expert {

using json = nlohmann::json;

std::string environment_url(const std::string& name) {
    if (name == "free") return "https://free.as2expert.com/api/v1";
    if (name == "b2b") return "https://b2b.as2expert.com/api/v1";
    return "";
}

namespace {
json as_array(json v) {
    if (v.is_array()) return v;
    if (v.is_null()) return json::array();
    return json::array({std::move(v)});
}

std::vector<std::uint8_t> decode_b64_field(const json& data) {
    std::string b64;
    if (data.is_object()) {
        for (const char* k : {"content_b64", "contenido_base64"}) {
            auto it = data.find(k);
            if (it != data.end() && it->is_string()) { b64 = it->get<std::string>(); break; }
        }
    }
    std::vector<std::uint8_t> out;
    if (!b64.empty() && !detail::base64_decode(b64, out))
        throw TransportError("bad base64 in response");
    return out;
}
}  // namespace

// ---- Messages ---------------------------------------------------------------
json Messages::list(const json& filter) { return as_array(t_->post("/messages", filter)); }
json Messages::folders(const json& filter) { return as_array(t_->post("/messages/folders", filter)); }
json Messages::get(const json& id) { return t_->post("/messages/detail", {{"id", id}}); }

std::vector<std::uint8_t> Messages::download(const json& id) {
    return decode_b64_field(t_->post("/messages/download", {{"id", id}}));
}

json Messages::send(const json& partner, const std::string& subject,
                    const std::string& file_name, const std::vector<std::uint8_t>& content) {
    return t_->post("/messages/send", {{"partner", partner},
                                       {"subject", subject},
                                       {"file_name", file_name},
                                       {"file_content", detail::base64_encode(content)}});
}
json Messages::send(const json& partner, const std::string& subject,
                    const std::string& file_name, const std::string& content) {
    return t_->post("/messages/send", {{"partner", partner},
                                       {"subject", subject},
                                       {"file_name", file_name},
                                       {"file_content", detail::base64_encode(content)}});
}
json Messages::mark_read(const json& id) { return t_->post("/messages/mark-read", {{"id", id}}); }
json Messages::mark_unread(const json& id) { return t_->post("/messages/mark-unread", {{"id", id}}); }
json Messages::move_to(const json& id, const json& folder) {
    return t_->post("/messages/move", {{"id", id}, {"folder", folder}});
}
json Messages::remove(const json& id) { return t_->post("/messages/delete", {{"id", id}}); }
json Messages::changes(const json& params) { return t_->post("/messages/changes", params); }

// ---- Partners ---------------------------------------------------------------
json Partners::list(const json& filter) { return as_array(t_->post("/partners", filter)); }
json Partners::get(const json& id) { return t_->post("/partners/detail", {{"id", id}}); }
json Partners::create(const json& partner) { return t_->post("/partners/create", partner); }

// ---- Certificates -----------------------------------------------------------
json Certificates::list() { return as_array(t_->post("/certificates", json::object())); }
json Certificates::get(const json& id) { return t_->post("/certificates/detail", {{"id", id}}); }
json Certificates::create(const json& cert) { return t_->post("/certificates/create", cert); }

// ---- Stations ---------------------------------------------------------------
json Stations::list(const json& filter) { return as_array(t_->post("/stations", filter)); }
json Stations::get(const json& id) { return t_->post("/stations/detail", {{"id", id}}); }
json Stations::stats(const json& id) { return t_->post("/stations/stats", {{"id", id}}); }
json Stations::create(const json& station) { return t_->post("/stations/create", station); }

// ---- Webhooks ---------------------------------------------------------------
json Webhooks::configure(const json& config) { return t_->post("/webhooks/configure", config); }
json Webhooks::get() { return t_->post("/webhooks/get", json::object()); }
json Webhooks::test() { return t_->post("/webhooks/test", json::object()); }
json Webhooks::logs(const json& params) { return t_->post("/webhooks/logs", params); }

// ---- BusinessDocuments ------------------------------------------------------
json BusinessDocuments::create(const json& document, const std::string& idempotency_key) {
    if (idempotency_key.empty())
        return t_->post("/business-documents", document);
    return t_->post("/business-documents", document, {{"Idempotency-Key", idempotency_key}});
}
json BusinessDocuments::get(const json& business_document_id) {
    return t_->post("/business-documents/detail", {{"business_document_id", business_document_id}});
}
json BusinessDocuments::diagnostics(const json& params) {
    return t_->post("/business-documents/diagnostics", params);
}

// ---- Edifact ----------------------------------------------------------------
json Edifact::analyze(const std::string& edifact) {
    return t_->post("/edifact/analyze", {{"edifact", edifact}});
}
json Edifact::validate(const std::string& edifact) { return analyze(edifact); }
json Edifact::convert(const std::string& edifact, const std::string& format) {
    return t_->post("/edifact/convert", {{"edifact", edifact}, {"format", format}, {"sequence", 1}});
}
json Edifact::acknowledge(const std::string& edifact, const std::string& kind,
                          bool acknowledged, const json& errors) {
    return t_->post("/edifact/acknowledge", {{"edifact", edifact},
                                             {"kind", kind},
                                             {"acknowledged", acknowledged},
                                             {"errors", errors}});
}
json Edifact::skeleton(const std::string& message_type, const std::string& release, bool compose) {
    return t_->post("/edifact/skeleton",
                    {{"message_type", message_type}, {"release", release}, {"compose", compose}});
}

// ---- Dashboard --------------------------------------------------------------
json Dashboard::kpis() { return t_->post("/dashboard/kpis", json::object()); }

// ---- Client -----------------------------------------------------------------
Client::Client(Config config)
    : t_(std::make_shared<Transport>(std::move(config))),
      messages_(t_),
      partners_(t_),
      certificates_(t_),
      stations_(t_),
      webhooks_(t_),
      business_documents_(t_),
      edifact_(t_),
      dashboard_(t_) {}

Client::~Client() = default;
Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

const std::string& Client::base_url() const noexcept { return t_->base_url(); }

}  // namespace as2expert
