#pragma once

#include "uno/app/ports.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
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

// Time under the test's control: timers fire when the clock is advanced past them, in order, each one seeing the clock
// at its own due time. It is also the Clock of the application, so deadlines and timers agree.
class ManualScheduler final : public app::Scheduler {
public:
    explicit ManualScheduler(ManualClock& clock) noexcept : clock_(&clock) {}

    [[nodiscard]] app::TimerHandle schedule(std::chrono::milliseconds delay, std::function<void()> callback) override
    {
        const app::TimerHandle handle{++lastId_};
        timers_.push_back(
            Timer{.id = handle.id, .due = clock_->nowMillis() + delay.count(), .callback = std::move(callback)});
        return handle;
    }

    void cancel(app::TimerHandle timer) override
    {
        std::erase_if(timers_, [&timer](const Timer& entry) { return entry.id == timer.id; });
    }

    // Moves time forward, firing every timer that falls due on the way (a callback may schedule new ones).
    void advance(std::chrono::milliseconds duration)
    {
        const std::int64_t target = clock_->nowMillis() + duration.count();
        while (true) {
            const auto next = std::ranges::min_element(timers_, {}, &Timer::due);
            if (next == timers_.end() || next->due > target) {
                break;
            }
            Timer fired = std::move(*next);
            timers_.erase(next);
            clock_->advanceMillis(std::max<std::int64_t>(0, fired.due - clock_->nowMillis()));
            fired.callback();
        }
        clock_->advanceMillis(target - clock_->nowMillis());
    }

    [[nodiscard]] std::size_t pendingCount() const noexcept { return timers_.size(); }

private:
    struct Timer {
        std::uint64_t id{};
        std::int64_t due{};
        std::function<void()> callback;
    };

    ManualClock* clock_;
    std::uint64_t lastId_ = 0;
    std::vector<Timer> timers_;
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
