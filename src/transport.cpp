#include "transport.hpp"

#include <atomic>
#include <thread>

#include <curl/curl.h>

#include "as2expert/error.hpp"

namespace as2expert {
namespace {

using nlohmann::json;

// One-time libcurl global init, torn down at process exit. RAII guard.
struct CurlGlobal {
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};
void ensure_curl_global() {
    static CurlGlobal g;
    (void)g;
}

// RAII wrappers for libcurl handles.
struct EasyHandle {
    CURL* h;
    EasyHandle() : h(curl_easy_init()) {}
    ~EasyHandle() { if (h) curl_easy_cleanup(h); }
    EasyHandle(const EasyHandle&) = delete;
    EasyHandle& operator=(const EasyHandle&) = delete;
};
struct Slist {
    curl_slist* h = nullptr;
    ~Slist() { if (h) curl_slist_free_all(h); }
    void add(const std::string& s) { h = curl_slist_append(h, s.c_str()); }
};

std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

bool should_retry(long status) { return status == 429 || status >= 500; }

std::string message_of(const json& payload, const std::string& fallback) {
    if (payload.is_object()) {
        for (const char* k : {"msg", "message", "error"}) {
            auto it = payload.find(k);
            if (it != payload.end() && it->is_string()) {
                std::string s = it->get<std::string>();
                if (!s.empty()) return s;
            }
        }
    }
    if (!fallback.empty()) return fallback.substr(0, 300);
    return "request failed";
}

[[noreturn]] void raise(long status, const std::string& message, json payload) {
    std::optional<std::string> code;
    if (payload.is_object()) {
        auto it = payload.find("code");
        if (it != payload.end() && it->is_string()) code = it->get<std::string>();
    }
    int s = static_cast<int>(status);
    if (status == 401 || status == 403) throw AuthError(message, s, payload, code);
    if (status == 400 || status == 422) {
        json fields = json::array();
        if (payload.is_object() && payload.contains("fields")) fields = payload["fields"];
        throw ValidationError(message, s, payload, code, fields);
    }
    if (status == 404) throw NotFoundError(message, s, payload, code);
    if (status == 429) {
        std::optional<double> ra;
        if (payload.is_object() && payload.contains("retry_after") &&
            payload["retry_after"].is_number())
            ra = payload["retry_after"].get<double>();
        throw RateLimitError(message, s, payload, code, ra);
    }
    if (status >= 500) throw ServerError(message, s, payload, code);
    throw ApiError(message, s, payload, code);
}

json unwrap_data(const json& payload) {
    if (payload.is_object()) {
        auto it = payload.find("data");
        if (it != payload.end()) return *it;
    }
    return payload;
}

}  // namespace

Transport::Transport(Config cfg) : cfg_(std::move(cfg)) {
    ensure_curl_global();
    base_url_ = !cfg_.base_url.empty() ? cfg_.base_url : environment_url(cfg_.environment);
    while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
    if (base_url_.empty()) {
        throw ApiError("set Config::base_url or Config::environment (\"free\"|\"b2b\")");
    }
    if (cfg_.token.empty()) {
        throw ApiError("Config::token is required");
    }
    user_agent_ = cfg_.user_agent.empty() ? std::string("as2expert-cpp/0.1.0") : cfg_.user_agent;
}

json Transport::post(const std::string& path, const json& body) {
    return post(path, body, {});
}

json Transport::post(const std::string& path, const json& body,
                     const std::vector<std::pair<std::string, std::string>>& headers) {
    std::string p = path;
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    const std::string url = base_url_ + "/" + p;
    const std::string payload = body.dump();

    for (int attempt = 0;; ++attempt) {
        EasyHandle eh;
        if (!eh.h) throw TransportError("failed to init libcurl handle");

        std::string resp;
        Slist hdrs;
        hdrs.add("Content-Type: application/json");
        hdrs.add("Accept: application/json");
        hdrs.add("Authorization: Bearer " + cfg_.token);
        for (const auto& kv : headers) hdrs.add(kv.first + ": " + kv.second);

        curl_easy_setopt(eh.h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(eh.h, CURLOPT_POST, 1L);
        curl_easy_setopt(eh.h, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(eh.h, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
        curl_easy_setopt(eh.h, CURLOPT_HTTPHEADER, hdrs.h);
        curl_easy_setopt(eh.h, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(eh.h, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(eh.h, CURLOPT_USERAGENT, user_agent_.c_str());
        curl_easy_setopt(eh.h, CURLOPT_TIMEOUT_MS, static_cast<long>(cfg_.timeout.count()));
        curl_easy_setopt(eh.h, CURLOPT_NOSIGNAL, 1L);
        if (!cfg_.verify_tls) {
            curl_easy_setopt(eh.h, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(eh.h, CURLOPT_SSL_VERIFYHOST, 0L);
        }

        CURLcode rc = curl_easy_perform(eh.h);
        if (rc != CURLE_OK) {
            bool transient = rc == CURLE_OPERATION_TIMEDOUT || rc == CURLE_COULDNT_CONNECT ||
                             rc == CURLE_GOT_NOTHING || rc == CURLE_RECV_ERROR ||
                             rc == CURLE_SEND_ERROR;
            if (transient && attempt < cfg_.max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200 * (1 << attempt)));
                continue;
            }
            throw TransportError(std::string("curl error: ") + curl_easy_strerror(rc));
        }

        long status = 0;
        curl_easy_getinfo(eh.h, CURLINFO_RESPONSE_CODE, &status);
        json parsed = json::parse(resp, nullptr, false);
        json payload_json = parsed.is_discarded() ? json() : parsed;

        if (status >= 200 && status < 300) {
            return unwrap_data(payload_json);
        }
        if (should_retry(status) && attempt < cfg_.max_retries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200 * (1 << attempt)));
            continue;
        }
        raise(status, message_of(payload_json, resp), payload_json);
    }
}

}  // namespace as2expert
