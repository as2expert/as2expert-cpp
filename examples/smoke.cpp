// E2E smoke test: AS2EXPERT_TOKEN=... ./smoke
#include <cstdlib>
#include <iostream>

#include "as2expert/client.hpp"

int main() {
    const char* tok = std::getenv("AS2EXPERT_TOKEN");
    if (!tok) {
        std::cerr << "set AS2EXPERT_TOKEN\n";
        return 2;
    }
    as2expert::Config cfg;
    cfg.token = tok;
    cfg.environment = "free";

    try {
        as2expert::Client client(cfg);
        std::cout << "base_url: " << client.base_url() << "\n";

        const std::string edi =
            "UNB+UNOC:3+A+B+260830:1000+1'UNH+1+ORDERS:D:96A:UN'BGM+220+PO-CPP'UNT+2+1'UNZ+1+1'";
        auto out = client.edifact().convert(edi, "json");
        std::cout << "convert -> filename=" << out.value("filename", "")
                  << " content_len=" << out.value("content", std::string()).size() << "\n";

        auto ack = client.edifact().acknowledge(
            "UNA:+.?*'UNB+UNOC:3+A:14+B:14+260830:1000+X9'UNH+M1+ORDERS:D:96A:UN'"
            "BGM+220+P'UNT+2+M1'UNZ+1+X9'");
        std::cout << "acknowledge -> kind=" << ack.value("kind", "")
                  << " ctrl=" << ack.value("control_reference", "") << "\n";

        auto msgs = client.messages().list({{"limit", 3}});
        std::cout << "messages.list -> " << msgs.size() << " items\n";
    } catch (const as2expert::ApiError& e) {
        std::cerr << "API error";
        if (e.status()) std::cerr << " (" << *e.status() << ")";
        std::cerr << ": " << e.what() << "\n";
        return 1;
    }
    return 0;
}
