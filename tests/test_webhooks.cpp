// Unit tests for webhook signature verification (no network).
#include <cstdio>

#include "as2expert/client.hpp"

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

int main() {
    using namespace as2expert;
    const std::string secret = "secret-0123456789abcdef";
    const std::string body = "{\"a\":1}";
    const std::string sig = sign_payload(secret, "1000", body);

    CHECK(sig.rfind("sha256=", 0) == 0);
    // matches within tolerance
    CHECK(verify_signature(secret, "1000", body, sig, 300, 1000));
    // accepts a bare hex signature (no prefix)
    CHECK(verify_signature(secret, "1000", body, sig.substr(7), 300, 1000));
    // stale timestamp rejected
    CHECK(!verify_signature(secret, "1000", body, sig, 300, 99999));
    // tampered body rejected
    CHECK(!verify_signature(secret, "1000", "{\"a\":2}", sig, 300, 1000));
    // bad timestamp / empty inputs rejected
    CHECK(!verify_signature(secret, "not-a-number", body, sig, 300, 1000));
    CHECK(!verify_signature("", "1000", body, sig, 300, 1000));
    CHECK(!verify_signature(secret, "1000", body, "", 300, 1000));

    // base64 round-trip via a real send() body would need network; the encoder is
    // exercised indirectly by convert/acknowledge in the smoke example.

    if (failures == 0) std::printf("all webhook tests passed\n");
    return failures == 0 ? 0 : 1;
}
