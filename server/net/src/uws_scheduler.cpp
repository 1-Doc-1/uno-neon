#include "uno/net/uws_scheduler.hpp"

#include <libusockets.h>
#include <uwebsockets/App.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>

namespace uno::net {
namespace {

struct TimerPayload {
    void* impl = nullptr;
    std::uint64_t id = 0;
};

} // namespace

struct UwsScheduler::Impl {
    struct Entry {
        us_timer_t* timer = nullptr;
        std::function<void()> callback;
    };

    static void onTimer(us_timer_t* timer)
    {
        const auto payload = *static_cast<TimerPayload*>(us_timer_ext(timer));
        auto* const impl = static_cast<Impl*>(payload.impl);
        const auto found = impl->entries.find(payload.id);
        if (found == impl->entries.end()) {
            return;
        }
        // One-shot: forget the timer before running the callback, which may schedule or cancel others.
        const auto callback = std::move(found->second.callback);
        impl->entries.erase(found);
        us_timer_close(timer);
        callback();
    }

    std::uint64_t lastId = 0;
    bool shutDown = false;
    std::unordered_map<std::uint64_t, Entry> entries;
};

UwsScheduler::UwsScheduler() : impl_(std::make_unique<Impl>()) {}

UwsScheduler::~UwsScheduler()
{
    cancelAll();
}

app::TimerHandle UwsScheduler::schedule(std::chrono::milliseconds delay, std::function<void()> callback)
{
    const std::uint64_t id = ++impl_->lastId;
    if (impl_->shutDown) {
        return app::TimerHandle{id}; // no timer: it would keep the loop alive, and run() would never return
    }
    // uSockets types the loop of uWebSockets as its own struct: its API asks for this cast.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* const loop = reinterpret_cast<us_loop_t*>(uWS::Loop::get());
    // fallthrough = 0: a timer counts as an active handle of the loop. With fallthrough = 1, closing the timer would
    // make the epoll backend of uSockets count it out twice, and the loop would never end (the Linux hang of 2.5).
    us_timer_t* const timer = us_create_timer(loop, 0, sizeof(TimerPayload));
    *static_cast<TimerPayload*>(us_timer_ext(timer)) = TimerPayload{.impl = impl_.get(), .id = id};
    impl_->entries.emplace(id, Impl::Entry{.timer = timer, .callback = std::move(callback)});
    us_timer_set(timer, &Impl::onTimer, static_cast<int>(std::max<std::chrono::milliseconds::rep>(delay.count(), 1)),
                 0);
    return app::TimerHandle{id};
}

void UwsScheduler::cancelAll()
{
    impl_->shutDown = true;
    for (const auto& [id, entry] : impl_->entries) {
        us_timer_close(entry.timer);
    }
    impl_->entries.clear();
}

void UwsScheduler::cancel(app::TimerHandle timer)
{
    const auto found = impl_->entries.find(timer.id);
    if (found == impl_->entries.end()) {
        return;
    }
    us_timer_close(found->second.timer);
    impl_->entries.erase(found);
}

} // namespace uno::net
