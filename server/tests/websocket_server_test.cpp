#include "uno/app/client_message.hpp"
#include "uno/app/error_code.hpp"
#include "uno/app/server_message.hpp"
#include "uno/net/codec.hpp"
#include "uno/net/crypto_runtime.hpp"
#include "uno/net/origin_policy.hpp"
#include "uno/net/websocket_server.hpp"

#include "support/require.hpp"
#include "support/running_server.hpp"
#include "support/test_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <variant>

// Integration tests of the real server, through real sockets (SPEC §17, §9.4).

namespace {

using uno::testing::RecordingHandler;
using uno::testing::require;
using uno::testing::RunningServer;
using uno::testing::TestWebSocket;

constexpr const char* kAllowedOrigin = "http://localhost:4200";
constexpr int kSwitchingProtocols = 101;
constexpr std::size_t kOneMebibyte = std::size_t{1024} * 1024;

uno::net::WebSocketServerConfig serverConfig(std::size_t rooms, const uno::net::RateLimits& limits)
{
    uno::net::WebSocketServerConfig config;
    config.rateLimits = limits;
    config.originPolicy = uno::net::OriginPolicy({kAllowedOrigin});
    config.roomCount = [rooms] { return rooms; };
    return config;
}

struct Fixture {
    RecordingHandler handler;
    RunningServer server;

    explicit Fixture(std::size_t rooms = 0, const uno::net::RateLimits& limits = {})
        : server(serverConfig(rooms, limits), handler,
                 [this](uno::net::WebSocketServer& running) { handler.attach(running); })
    {
    }

    TestWebSocket connect(const char* origin = kAllowedOrigin) const
    {
        int status = 0;
        auto socket = TestWebSocket::connect(server.port(), origin, status);
        REQUIRE(status == kSwitchingProtocols);
        return require(std::move(socket));
    }

    // The status of a handshake that is not expected to succeed.
    int handshakeStatus(const char* origin) const
    {
        int status = 0;
        const auto socket = TestWebSocket::connect(server.port(), origin, status);
        REQUIRE(socket.has_value());
        return status;
    }
};

std::string drawCardRequest(const std::string& id)
{
    return R"({"v":1,"id":")" + id + R"(","type":"game.drawCard","payload":{}})";
}

uno::app::response::Message decodeReply(TestWebSocket& socket)
{
    return uno::net::decodeServerMessage(require(socket.receiveText())).value();
}

} // namespace

TEST_CASE("GET /health reports the state of the server", "[net][server]")
{
    REQUIRE(uno::net::initializeCryptoRuntime());
    const Fixture fixture(3);
    const auto socket = fixture.connect();

    const auto response = require(uno::testing::httpGet(fixture.server.port(), "/health"));

    REQUIRE(response.status == 200);
    const auto body = nlohmann::json::parse(response.body);
    REQUIRE(body.at("status") == "ok");
    REQUIRE(body.at("rooms") == 3);
    REQUIRE(body.at("connections") == 1);
    REQUIRE(body.at("uptimeSeconds").is_number_unsigned());
}

TEST_CASE("Any other HTTP path is a 404", "[net][server]")
{
    const Fixture fixture;

    const auto response = require(uno::testing::httpGet(fixture.server.port(), "/admin"));

    REQUIRE(response.status == 404);
}

TEST_CASE("A well-formed request reaches the handler and its reply comes back", "[net][server]")
{
    const Fixture fixture;
    auto socket = fixture.connect();

    REQUIRE(socket.send(drawCardRequest("c-7")));
    const auto reply = decodeReply(socket);

    REQUIRE(std::get<uno::app::response::Ack>(reply).replyTo == "c-7");
    const auto requests = fixture.handler.requests();
    REQUIRE(requests.size() == 1);
    REQUIRE(std::holds_alternative<uno::app::request::DrawCard>(requests.front().body));
}

TEST_CASE("A foreign Origin is refused at the handshake", "[net][server][origin]")
{
    const Fixture fixture;

    REQUIRE(fixture.handshakeStatus("http://evil.example") == 403);
}

TEST_CASE("A handshake without Origin is refused", "[net][server][origin]")
{
    const Fixture fixture;

    REQUIRE(fixture.handshakeStatus("") == 403);
}

TEST_CASE("Malformed messages get a typed error and the connection stays open", "[net][server]")
{
    const Fixture fixture;
    auto socket = fixture.connect();

    REQUIRE(socket.send("{ not json"));
    REQUIRE(socket.send(R"({"v":1,"id":"c-2","type":"cheat.revealHands","payload":{}})"));
    REQUIRE(socket.send(drawCardRequest("c-3")));

    const auto first = std::get<uno::app::response::Error>(decodeReply(socket));
    const auto second = std::get<uno::app::response::Error>(decodeReply(socket));
    const auto third = decodeReply(socket);
    REQUIRE(first.code == uno::app::ErrorCode::MalformedMessage);
    REQUIRE_FALSE(first.replyTo.has_value());
    REQUIRE(second.code == uno::app::ErrorCode::UnknownType);
    REQUIRE(second.replyTo == "c-2");
    REQUIRE(std::holds_alternative<uno::app::response::Ack>(third));
}

TEST_CASE("The tenth malformed message closes the connection with 1008", "[net][server]")
{
    const Fixture fixture;
    auto socket = fixture.connect();

    for (unsigned count = 0; count < uno::net::kMaxMalformedMessages; ++count) {
        REQUIRE(socket.send("garbage"));
    }
    const auto closure = require(socket.waitForClosure());

    REQUIRE(closure.code == 1008);
    REQUIRE(fixture.handler.waitForDisconnections(1));
}

TEST_CASE("A frame over 4 KiB closes the connection with 1009", "[net][server]")
{
    const Fixture fixture;
    auto socket = fixture.connect();

    REQUIRE(socket.send(std::string(uno::net::kMaxMessageBytes + 1, 'x')));
    const auto closure = require(socket.waitForClosure());

    REQUIRE(closure.code == 1009);
    REQUIRE(fixture.handler.requests().empty());
}

TEST_CASE("A frame of exactly 4 KiB is still read", "[net][server]")
{
    const Fixture fixture;
    auto socket = fixture.connect();

    REQUIRE(socket.send(std::string(uno::net::kMaxMessageBytes, 'x')));
    const auto reply = std::get<uno::app::response::Error>(decodeReply(socket));

    REQUIRE(reply.code == uno::app::ErrorCode::MalformedMessage);
}

TEST_CASE("A huge frame is cut off by the transport itself", "[net][server]")
{
    const Fixture fixture;
    auto socket = fixture.connect();

    static_cast<void>(socket.send(std::string(kOneMebibyte, 'x'))); // the server may cut us off mid-send
    static_cast<void>(require(socket.waitForClosure()));

    REQUIRE(fixture.handler.waitForDisconnections(1));
    REQUIRE(fixture.handler.requests().empty());
}

TEST_CASE("A binary frame closes the connection with 1003", "[net][server]")
{
    const Fixture fixture;
    auto socket = fixture.connect();

    constexpr int kBinary = 0x2;
    REQUIRE(socket.send("abc", kBinary));
    const auto closure = require(socket.waitForClosure());

    REQUIRE(closure.code == 1003);
}

TEST_CASE("Each closed connection is reported to the handler", "[net][server]")
{
    const Fixture fixture;
    {
        auto socket = fixture.connect();
        REQUIRE(socket.send(drawCardRequest("c-1")));
        REQUIRE(socket.receiveText().has_value());
    }

    REQUIRE(fixture.handler.waitForDisconnections(1));
}

namespace {

std::string createRoomRequest(const std::string& id)
{
    return R"({"v":1,"id":")" + id + R"(","type":"room.create","payload":{"nickname":"Léa"}})";
}

std::string joinRoomRequest(const std::string& id)
{
    return R"({"v":1,"id":")" + id + R"(","type":"room.join","payload":{"code":"ABCDEF","nickname":"Max"}})";
}

uno::net::RateLimits limits(double burst, double perSecond, std::size_t creates = 100, std::size_t joins = 100)
{
    return uno::net::RateLimits{
        .burst = burst,
        .perSecond = perSecond,
        .createsPerMinute = creates,
        .joinsPerMinute = joins,
    };
}

} // namespace

TEST_CASE("A connection that sends faster than its bucket allows is told RATE_LIMITED", "[net][server][rate]")
{
    const Fixture fixture(0, limits(5, 0.001));
    auto socket = fixture.connect();

    for (int index = 0; index < 8; ++index) {
        REQUIRE(socket.send(drawCardRequest("c-" + std::to_string(index))));
    }

    int acknowledged = 0;
    int limited = 0;
    for (int index = 0; index < 8; ++index) {
        const auto reply = decodeReply(socket);
        if (std::holds_alternative<uno::app::response::Ack>(reply)) {
            ++acknowledged;
        } else if (std::get<uno::app::response::Error>(reply).code == uno::app::ErrorCode::RateLimited) {
            ++limited;
        }
    }
    REQUIRE(acknowledged == 5);
    REQUIRE(limited == 3);
    REQUIRE(fixture.handler.requests().size() == 5);
}

TEST_CASE("A connection that keeps flooding is closed with 1008", "[net][server][rate]")
{
    const Fixture fixture(0, limits(3, 0.001));
    auto socket = fixture.connect();

    for (int index = 0; index < 20; ++index) {
        static_cast<void>(socket.send("garbage"));
    }
    const auto closure = require(socket.waitForClosure());

    REQUIRE(closure.code == 1008);
}

TEST_CASE("Room creation is limited per address, across connections", "[net][server][rate]")
{
    const Fixture fixture(0, limits(100, 100, 2));
    auto first = fixture.connect();
    auto second = fixture.connect();

    // One at a time: two sockets give no guarantee about which request the server reads first.
    REQUIRE(first.send(createRoomRequest("c-1")));
    REQUIRE(std::holds_alternative<uno::app::response::Ack>(decodeReply(first)));
    REQUIRE(second.send(createRoomRequest("c-2")));
    REQUIRE(std::holds_alternative<uno::app::response::Ack>(decodeReply(second)));
    REQUIRE(first.send(createRoomRequest("c-3")));
    const auto refused = std::get<uno::app::response::Error>(decodeReply(first));
    REQUIRE(refused.code == uno::app::ErrorCode::RateLimited);
    REQUIRE(refused.replyTo == "c-3");
    REQUIRE(fixture.handler.requests().size() == 2);
}

TEST_CASE("Join attempts are limited per address, so room codes cannot be guessed", "[net][server][rate]")
{
    const Fixture fixture(0, limits(100, 100, 100, 3));
    auto socket = fixture.connect();

    for (int index = 0; index < 5; ++index) {
        REQUIRE(socket.send(joinRoomRequest("c-" + std::to_string(index))));
    }

    int limited = 0;
    for (int index = 0; index < 5; ++index) {
        const auto reply = decodeReply(socket);
        limited += std::holds_alternative<uno::app::response::Error>(reply) ? 1 : 0;
    }
    REQUIRE(limited == 2);
    REQUIRE(fixture.handler.requests().size() == 3);
}
