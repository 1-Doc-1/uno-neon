#include "uno/app/id_generator.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace uno::app {
namespace {

constexpr std::string_view kBase64Url = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr std::string_view kRoomCodeAlphabet = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
constexpr std::size_t kTokenLength = 22;
constexpr std::size_t kPlayerIdSuffixLength = 10;
constexpr std::size_t kRoomCodeLength = 6;

std::string randomString(core::RandomSource& random, std::string_view alphabet, std::size_t length)
{
    std::string text;
    text.reserve(length);
    for (std::size_t index = 0; index < length; ++index) {
        text.push_back(alphabet.at(random.uniform(static_cast<std::uint32_t>(alphabet.size()))));
    }
    return text;
}

} // namespace

SessionToken generateSessionToken(core::RandomSource& random)
{
    return SessionToken{randomString(random, kBase64Url, kTokenLength)};
}

core::PlayerId generatePlayerId(core::RandomSource& random)
{
    return core::PlayerId{"p_" + randomString(random, kBase64Url, kPlayerIdSuffixLength)};
}

RoomCode generateRoomCode(core::RandomSource& random, const std::function<bool(const RoomCode&)>& isTaken)
{
    while (true) {
        RoomCode code{randomString(random, kRoomCodeAlphabet, kRoomCodeLength)};
        if (!isTaken(code)) {
            return code;
        }
    }
}

} // namespace uno::app
