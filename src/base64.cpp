#include "base64.hpp"

namespace as2expert::detail {

namespace {
constexpr char kEnc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int dec_val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
}  // namespace

std::string base64_encode(const std::uint8_t* data, std::size_t len) {
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < len; i += 3) {
        std::uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(kEnc[(n >> 18) & 63]);
        out.push_back(kEnc[(n >> 12) & 63]);
        out.push_back(kEnc[(n >> 6) & 63]);
        out.push_back(kEnc[n & 63]);
    }
    if (i < len) {
        std::uint32_t n = data[i] << 16;
        bool two = (i + 1 < len);
        if (two) n |= data[i + 1] << 8;
        out.push_back(kEnc[(n >> 18) & 63]);
        out.push_back(kEnc[(n >> 12) & 63]);
        out.push_back(two ? kEnc[(n >> 6) & 63] : '=');
        out.push_back('=');
    }
    return out;
}

bool base64_decode(const std::string& in, std::vector<std::uint8_t>& out) {
    out.clear();
    int buf = 0, bits = 0;
    for (unsigned char c : in) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        int v = dec_val(c);
        if (v < 0) return false;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return true;
}

}  // namespace as2expert::detail
