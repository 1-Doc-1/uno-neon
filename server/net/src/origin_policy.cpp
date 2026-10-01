#include "uno/net/origin_policy.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uno::net {
namespace {

std::string lowercase(std::string_view text)
{
    std::string lowered(text);
    std::ranges::transform(lowered, lowered.begin(), [](char character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
    return lowered;
}

} // namespace

OriginPolicy::OriginPolicy(std::vector<std::string> allowedOrigins) : allowedOrigins_(std::move(allowedOrigins))
{
    std::ranges::transform(allowedOrigins_, allowedOrigins_.begin(),
                           [](const std::string& origin) { return lowercase(origin); });
}

bool OriginPolicy::allows(std::string_view origin) const
{
    return !origin.empty() && std::ranges::contains(allowedOrigins_, lowercase(origin));
}

} // namespace uno::net
