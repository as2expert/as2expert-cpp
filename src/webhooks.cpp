#include "as2expert/client.hpp"

#include <cctype>
#include <cstdlib>

#include <openssl/hmac.h>

namespace as2expert {
namespace {

std::string to_hex(const unsigned char* data, unsigned len) {
    static const char* h = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (unsigned i = 0; i < len; ++i) {
        out.push_back(h[data[i] >> 4]);
        out.push_back(h[data[i] & 0xF]);
    }
    return out;
}

// Length-constant comparison for equal-length strings.
bool constant_time_eq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return diff == 0;
}

}  // namespace

std::string sign_payload(const std::string& secret, const std::string& timestamp,
                         const std::string& body) {
    const std::string msg = timestamp + "." + body;
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(),
         secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         mac, &len);
    return "sha256=" + to_hex(mac, len);
}

bool verify_signature(const std::string& secret, const std::string& timestamp,
                      const std::string& body, const std::string& signature,
                      long tolerance_secs, long now) {
    if (secret.empty() || signature.empty()) return false;

    // Parse the timestamp (trimmed) as an integer.
    std::size_t b = 0, e = timestamp.size();
    while (b < e && std::isspace(static_cast<unsigned char>(timestamp[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(timestamp[e - 1]))) --e;
    const std::string trimmed = timestamp.substr(b, e - b);
    if (trimmed.empty()) return false;
    char* end = nullptr;
    long ts = std::strtol(trimmed.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') return false;

    long delta = now - ts;
    if (delta < 0) delta = -delta;
    if (delta > tolerance_secs) return false;

    const std::string expected = sign_payload(secret, timestamp, body);
    const std::string provided =
        signature.rfind("sha256=", 0) == 0 ? signature : ("sha256=" + signature);
    return constant_time_eq(expected, provided);
}

}  // namespace as2expert
