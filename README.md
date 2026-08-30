# as2expert (C++)

Official C++ client for the **AS2Expert** REST API — send and receive AS2/EDI
messages, manage trading partners, certificates and stations, drive Business
Documents, and validate/convert **EDIFACT**.

- Idiomatic C++17 with RAII; no manual cleanup.
- Typed exception hierarchy, automatic retries on `429`/`5xx`, HMAC webhook
  verification.
- Configurable host: `free`, `b2b`, or any self-hosted deployment.

## Dependencies

- [libcurl](https://curl.se/libcurl/)
- [OpenSSL](https://www.openssl.org/) (crypto — for HMAC)
- [nlohmann/json](https://github.com/nlohmann/json) ≥ 3.9 (fetched automatically
  if not installed)
- CMake ≥ 3.16

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build          # unit tests (webhook HMAC), no network
```

Consume it from your own CMake project:

```cmake
find_package(as2expert REQUIRED)   # or add_subdirectory(as2expert-cpp)
target_link_libraries(myapp PRIVATE as2expert::as2expert)
```

## Quick start

```cpp
#include <as2expert/client.hpp>
#include <iostream>

int main() {
    as2expert::Config cfg;
    cfg.token = "YOUR_TOKEN";
    cfg.environment = "free";                 // or cfg.base_url = "https://your-host/api/v1";

    as2expert::Client client(cfg);

    // Send an EDI file to a partner (content is base64-encoded for you)
    client.messages().send("140", "Order 4711", "order.edi", std::string("UNB+...'"));

    // List and download inbound messages
    for (const auto& msg : client.messages().list({{"limit", 20}})) {
        std::cout << msg["id"] << " " << msg.value("asunto", "") << "\n";
        std::vector<std::uint8_t> bytes = client.messages().download(msg["id"]);
        (void)bytes;
    }
}
```

Every method maps to a single POST call (the API is POST-only) and returns
`as2expert::json` (an alias for `nlohmann::json`); list methods return a JSON
array.

## EDIFACT

```cpp
// Parse + validate + translate to JSON ("xml" / "text" also supported)
auto out = client.edifact().convert(raw_edi, "json");
std::cout << out["filename"] << " " << out["content"] << "\n";

// Build a functional acknowledgement (CONTRL / APERAK)
auto ack = client.edifact().acknowledge(raw_edi);
std::cout << ack["kind"] << " " << ack["control_reference"] << "\n";
```

## Errors

Every call throws a subclass of `as2expert::ApiError` on failure. Each carries
`status()`, `code()`, and `payload()`:

| Exception | When |
|-----------|------|
| `AuthError` | `401` / `403` |
| `ValidationError` | `400` / `422` (see `.fields()`) |
| `NotFoundError` | `404` |
| `RateLimitError` | `429` (see `.retry_after()`) |
| `ServerError` | `5xx` |
| `TransportError` | network/timeout, no HTTP status |

```cpp
try {
    client.business_documents().create(doc);
} catch (const as2expert::ValidationError& e) {
    std::cerr << "bad document: " << e.fields().dump() << "\n";
} catch (const as2expert::ApiError& e) {
    std::cerr << (e.status() ? *e.status() : 0) << " " << e.what() << "\n";
}
```

## Webhooks

AS2Expert signs deliveries with HMAC-SHA256 over `"<timestamp>.<body>"`, sent in
`X-AS2Expert-Timestamp` and `X-AS2Expert-Signature: sha256=<hex>`:

```cpp
#include <ctime>

bool ok = as2expert::verify_signature(
    secret,
    timestamp,                              // X-AS2Expert-Timestamp
    body,                                   // the exact raw request body
    signature,                              // X-AS2Expert-Signature
    as2expert::kDefaultToleranceSecs,
    static_cast<long>(std::time(nullptr)));
```

## Configuration

`as2expert::Config`:

- `token` (required)
- `base_url` **or** `environment` (`"free"` / `"b2b"`)
- `timeout` (`std::chrono::milliseconds`, default 30s)
- `max_retries` (default 2), `verify_tls` (default true), `user_agent`

## E2E smoke

```bash
AS2EXPERT_TOKEN=... ./build/smoke
```

## License

Apache-2.0 — see [LICENSE](LICENSE).
