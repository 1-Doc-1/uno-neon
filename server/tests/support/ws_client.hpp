#pragma once

// A player of the real server: the raw WebSocket test client plus the JSON codec, so a test sends typed requests
// and reads typed messages, as the Angular client will. The whole application runs behind the other end.

#include "uno/app/client_message.hpp"
#include "uno/app/server_message.hpp"
#include "uno/net/codec.hpp"

#include "support/require.hpp"
#include "support/test_client.hpp"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace uno::testing {

class WsClient {
public:
    WsClient(std::uint16_t port, const std::string& origin) : socket_(open(port, origin)) {}

    // Sends a request, returns the id it carries.
    std::string send(app::request::Body body)
    {
        std::string id = "c-" + std::to_string(++lastRequest_);
        const std::string text = net::encodeClientMessage(app::request::Envelope{.id = id, .body = std::move(body)});
        REQUIRE(socket_.send(text));
        return id;
    }

    // The next message of the server, in order, whatever it is.
    std::optional<app::response::Message> next(std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        if (!pending_.empty()) {
            auto message = std::move(pending_.front());
            pending_.pop_front();
            return message;
        }
        const auto text = socket_.receiveText(timeout);
        if (!text) {
            return std::nullopt;
        }
        auto decoded = net::decodeServerMessage(*text);
        REQUIRE(decoded.has_value()); // the server only ever speaks the protocol
        return std::move(*decoded);
    }

    // Waits for the next message of type T; the messages before it are kept for later (see next()).
    template <typename T>
    std::optional<T> await(std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        std::deque<app::response::Message> skipped;
        std::optional<T> found;
        while (auto message = next(timeout)) {
            if (const auto* typed = std::get_if<T>(&*message)) {
                found = *typed;
                break;
            }
            skipped.push_back(std::move(*message));
        }
        // Put what was skipped back in front of whatever came in the meantime.
        skipped.insert(skipped.end(), pending_.begin(), pending_.end());
        pending_ = std::move(skipped);
        return found;
    }

    // The answer to a request: the ack, or the error, skipping what the server pushed in between.
    std::optional<app::response::Message> answerTo(const std::string& id,
                                                   std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        std::deque<app::response::Message> skipped;
        std::optional<app::response::Message> answer;
        while (auto message = next(timeout)) {
            const auto* ack = std::get_if<app::response::Ack>(&*message);
            const auto* error = std::get_if<app::response::Error>(&*message);
            if ((ack != nullptr && ack->replyTo == id) || (error != nullptr && error->replyTo == id)) {
                answer = std::move(*message);
                break;
            }
            skipped.push_back(std::move(*message));
        }
        skipped.insert(skipped.end(), pending_.begin(), pending_.end());
        pending_ = std::move(skipped);
        return answer;
    }

    // Like answerTo, requiring an ack.
    void sendAndExpectAck(app::request::Body body)
    {
        const auto id = send(std::move(body));
        const auto answer = require(answerTo(id), "an answer");
        if (const auto* error = std::get_if<app::response::Error>(&answer)) {
            INFO("refused with error code " << static_cast<int>(error->code) << ", reason "
                                            << (error->reason ? static_cast<int>(*error->reason) : -1) << ": "
                                            << error->message);
        }
        REQUIRE(std::holds_alternative<app::response::Ack>(answer));
    }

    [[nodiscard]] TestWebSocket& socket() noexcept { return socket_; }

private:
    static TestWebSocket open(std::uint16_t port, const std::string& origin)
    {
        int status = 0;
        auto socket = TestWebSocket::connect(port, origin, status);
        REQUIRE(status == 101);
        return require(std::move(socket));
    }

    TestWebSocket socket_;
    std::deque<app::response::Message> pending_;
    std::uint64_t lastRequest_ = 0;
};

} // namespace uno::testing
