// Minimal base64 (standard alphabet) used by the client. Internal.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace as2expert::detail {

std::string base64_encode(const std::uint8_t* data, std::size_t len);
inline std::string base64_encode(const std::vector<std::uint8_t>& v) {
    return base64_encode(v.data(), v.size());
}
inline std::string base64_encode(const std::string& s) {
    return base64_encode(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

/// Decode standard base64. Ignores ASCII whitespace; returns false on invalid input.
bool base64_decode(const std::string& in, std::vector<std::uint8_t>& out);

}  // namespace as2expert::detail
