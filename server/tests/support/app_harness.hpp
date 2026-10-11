#pragma once

// The application wired to test doubles, plus a small vocabulary to act as players: tests read like a
// conversation (`alice.send(CreateRoom{...})`, `bob.lastError()`) instead of plumbing.

#include "uno/app/application.hpp"
#include "uno/app/client_message.hpp"
#include "uno/app/room_repository.hpp"
#include "uno/app/server_message.hpp"
#include "uno/testing/app_doubles.hpp"
#include "uno/testing/seeded_random_source.hpp"
#include "uno/testing/timeouts.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace uno::testing {

class AppHarness;

// One connected client: sends requests and reads what the server sent to its connection.
class TestPlayer {
public:
    TestPlayer(AppHarness& harness, app::ConnectionId connection, app::SessionToken token)
        : harness_(&harness), connection_(connection), token_(std::move(token))
    {
    }

    [[nodiscard]] app::ConnectionId connection() const noexcept { return connection_; }
    [[nodiscard]] const core::PlayerId& id() const { return playerId_; }
    [[nodiscard]] const app::SessionToken& token() const { return token_; }

    // Sends `session.hello` and remembers the identity from the welcome. With a token, the connection presents that
    // session's cookie (a returning browser); without, the one it was opened with.
    void hello(std::optional<app::SessionToken> token = std::nullopt);
    // Sends `session.takeOver` (the "play here" button of a second tab).
    void takeOver();
    // Sends a request and returns the id it was sent with.
    std::string send(app::request::Body body);

    // Everything received since the last call.
    [[nodiscard]] std::vector<app::response::Message> received();
    // The last message of type T received so far (not consuming).
    template <typename T>
    [[nodiscard]] std::optional<T> last() const;
    template <typename T>
    [[nodiscard]] std::vector<T> all() const;

    // The error answering the last request, if it failed.
    [[nodiscard]] std::optional<app::response::Error> lastError() const { return last<app::response::Error>(); }
    // The newest room.update seen.
    [[nodiscard]] app::response::RoomView room() const;

private:
    AppHarness* harness_;
    app::ConnectionId connection_;
    core::PlayerId playerId_;
    app::SessionToken token_;
    std::size_t consumed_ = 0;
    std::uint64_t lastRequest_ = 0;
};

class AppHarness {
public:
    explicit AppHarness(std::uint64_t seed = 7, app::Timeouts timeouts = withoutPresentationDelay(),
                        app::Limits limits = {})
        : random(seed), scheduler(clock), application(sink, rooms, random, clock, scheduler, timeouts, limits)
    {
    }

    // A new connection, from a browser that has no session cookie yet: the transport made a token for it.
    TestPlayer connect()
    {
        const app::ConnectionId connection{++lastConnection_};
        app::SessionToken token = freshToken();
        application.onConnected(connection, token);
        return {*this, connection, std::move(token)};
    }

    // A client that already said hello.
    TestPlayer helloPlayer()
    {
        auto player = connect();
        player.hello();
        return player;
    }

    SeededRandomSource random;
    ManualClock clock;
    ManualScheduler scheduler;
    app::InMemoryRoomRepository rooms;
    RecordingSink sink;
    app::Application application;

private:
    [[nodiscard]] app::SessionToken freshToken()
    {
        std::string digits = std::to_string(++lastToken_);
        return app::SessionToken{"T" + std::string(21 - digits.size(), '0') + digits};
    }

    std::uint64_t lastConnection_ = 0;
    std::uint64_t lastToken_ = 0;
};

inline void TestPlayer::hello(std::optional<app::SessionToken> token)
{
    if (token) {
        token_ = std::move(*token);
        harness_->application.onConnected(connection_, token_);
    }
    send(app::request::Hello{.clientVersion = "test"});
    if (const auto welcome = last<app::response::Welcome>()) {
        playerId_ = welcome->playerId;
    }
}

inline void TestPlayer::takeOver()
{
    send(app::request::TakeOver{});
    if (const auto welcome = last<app::response::Welcome>()) {
        playerId_ = welcome->playerId;
    }
}

inline std::string TestPlayer::send(app::request::Body body)
{
    std::string id = "r-" + std::to_string(++lastRequest_);
    harness_->application.onRequest(connection_, app::request::Envelope{.id = id, .body = std::move(body)});
    return id;
}

inline std::vector<app::response::Message> TestPlayer::received()
{
    // Only what was sent since the last call: a long match sends thousands of messages, and copying them all at each
    // step would make the tests quadratic.
    const auto& sent = harness_->sink.sent;
    std::vector<app::response::Message> fresh;
    for (const auto& entry : std::span(sent).subspan(consumed_)) {
        if (entry.connection == connection_) {
            fresh.push_back(entry.message);
        }
    }
    consumed_ = sent.size();
    return fresh;
}

template <typename T>
std::optional<T> TestPlayer::last() const
{
    const auto& sent = harness_->sink.sent;
    for (const auto& entry : std::views::reverse(sent)) {
        if (entry.connection == connection_) {
            if (const auto* typed = std::get_if<T>(&entry.message)) {
                return *typed;
            }
        }
    }
    return std::nullopt;
}

template <typename T>
std::vector<T> TestPlayer::all() const
{
    return harness_->sink.of<T>(connection_);
}

inline app::response::RoomView TestPlayer::room() const
{
    const auto update = last<app::response::RoomUpdate>();
    REQUIRE(update.has_value());
    return update->room;
}

} // namespace uno::testing
