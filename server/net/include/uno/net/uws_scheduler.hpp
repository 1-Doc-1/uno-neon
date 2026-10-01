#pragma once

#include "uno/app/ports.hpp"

#include <chrono>
#include <functional>
#include <memory>

namespace uno::net {

// The Scheduler port on the timers of the uWebSockets event loop: callbacks run on the loop thread, between
// two network events, so the application stays single-threaded. Timers never keep the loop alive by
// themselves: stopping the server ends it. Use it from the loop thread only, after the server exists.
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
