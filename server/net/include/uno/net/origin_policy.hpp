#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace uno::net {

// Which `Origin` headers may open a WebSocket (SPEC §9.4): the defence against another website
// driving a visitor's browser to our server. Browsers always send the header, so a request without
// one comes from a script or a tool and is refused too when a list is configured.
class OriginPolicy {
public:
    // `allowedOrigins` are exact origins ("https://uno.example.com", "http://localhost:4200"), compared
    // case-insensitively. There is no wildcard on purpose.
    explicit OriginPolicy(std::vector<std::string> allowedOrigins);

    [[nodiscard]] bool allows(std::string_view origin) const;

private:
    std::vector<std::string> allowedOrigins_;
};

} // namespace uno::net
