#pragma once

#include "uno/app/ports.hpp"

#include <chrono>
#include <functional>
#include <memory>

namespace uno::net {

// The Scheduler port on the timers of the uWebSockets event loop: callbacks run on the loop thread, between
// two network events, so the application stays single-threaded. A pending timer keeps the event loop running:
// cancelAll() must be called when the server stops (WebSocketServerConfig::onStop), or run() never returns.
// Use it from the loop thread only.
class UwsScheduler final : public app::Scheduler {
public:
    UwsScheduler();
    ~UwsScheduler() override;

    UwsScheduler(const UwsScheduler&) = delete;
    UwsScheduler& operator=(const UwsScheduler&) = delete;
    UwsScheduler(UwsScheduler&&) = delete;
    UwsScheduler& operator=(UwsScheduler&&) = delete;

    [[nodiscard]] app::TimerHandle schedule(std::chrono::milliseconds delay, std::function<void()> callback) override;
    void cancel(app::TimerHandle timer) override;
    // Cancels every pending timer. To be called on the loop thread when the server stops: a loop cannot be closed
    // while timers are still open.
    void cancelAll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace uno::net
