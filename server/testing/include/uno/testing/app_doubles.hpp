#pragma once

#include "uno/app/ports.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// Doubles for the ports of uno_app, so the application can be driven and observed without a network.
namespace uno::testing {

// A clock that only moves when the test says so.
class ManualClock final : public app::Clock {
public:
    explicit ManualClock(std::int64_t startMillis = 1'790'000'000'000) : now_(startMillis) {}

    [[nodiscard]] std::int64_t nowMillis() const override { return now_; }
    void advanceMillis(std::int64_t millis) { now_ += millis; }

private:
    std::int64_t now_;
};

// Records everything the application sends, in order.
class RecordingSink final : public app::MessageSink {
public:
    struct Sent {
        app::ConnectionId connection;
        app::response::Message message;
    };
    struct Closed {
        app::ConnectionId connection;
        int code{};
        std::string reason;
    };

    void send(app::ConnectionId connection, const app::response::Message& message) override
    {
        sent.push_back(Sent{.connection = connection, .message = message});
    }

    void close(app::ConnectionId connection, int code, std::string_view reason) override
    {
        closed.push_back(Closed{.connection = connection, .code = code, .reason = std::string(reason)});
    }

    // Every message sent to `connection`, oldest first.
    [[nodiscard]] std::vector<app::response::Message> to(app::ConnectionId connection) const
    {
        std::vector<app::response::Message> messages;
        for (const Sent& entry : sent) {
            if (entry.connection == connection) {
                messages.push_back(entry.message);
            }
        }
        return messages;
    }

    // The messages of type T sent to `connection`, oldest first.
    template <typename T>
    [[nodiscard]] std::vector<T> of(app::ConnectionId connection) const
    {
        std::vector<T> found;
        for (const Sent& entry : sent) {
            if (entry.connection == connection) {
                if (const auto* message = std::get_if<T>(&entry.message)) {
                    found.push_back(*message);
                }
            }
        }
        return found;
    }

    void clear()
    {
        sent.clear();
        closed.clear();
    }

    std::vector<Sent> sent;
    std::vector<Closed> closed;
};

} // namespace uno::testing
