#include "uno/app/ports.hpp"
#include "uno/net/uws_scheduler.hpp"

#include "support/running_server.hpp"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

using namespace std::chrono_literals;

// The Scheduler port on the real event loop. Timers need a running loop, which the test server provides.

namespace {

class Order {
public:
    void add(const std::string& name)
    {
        const std::scoped_lock lock(mutex_);
        fired_.push_back(name);
        changed_.notify_all();
    }

    bool waitFor(std::size_t count)
    {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, 2s, [&] { return fired_.size() >= count; });
    }

    std::vector<std::string> fired()
    {
        const std::scoped_lock lock(mutex_);
        return fired_;
    }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<std::string> fired_;
};

} // namespace

TEST_CASE("Timers fire in order of their delay, once, and a cancelled one never fires", "[net][scheduler]")
{
    Order order;
    uno::testing::RecordingHandler handler;
    uno::net::UwsScheduler scheduler;
    {
        const uno::testing::RunningServer server(
            uno::net::WebSocketServerConfig(), handler, [&](uno::net::WebSocketServer& running) {
                handler.attach(running);
                // On the loop thread, where timers belong.
                static_cast<void>(scheduler.schedule(60ms, [&order] { order.add("late"); }));
                static_cast<void>(scheduler.schedule(20ms, [&order] { order.add("early"); }));
                const auto cancelled = scheduler.schedule(30ms, [&order] { order.add("cancelled"); });
                scheduler.cancel(cancelled);
                scheduler.cancel(uno::app::TimerHandle{});
            });

        REQUIRE(order.waitFor(2));
        std::this_thread::sleep_for(100ms);
    }

    REQUIRE(order.fired() == std::vector<std::string>{"early", "late"});
}

TEST_CASE("A timer callback may schedule another timer", "[net][scheduler]")
{
    Order order;
    uno::testing::RecordingHandler handler;
    uno::net::UwsScheduler scheduler;
    {
        const uno::testing::RunningServer server(
            uno::net::WebSocketServerConfig(), handler, [&](uno::net::WebSocketServer& running) {
                handler.attach(running);
                static_cast<void>(scheduler.schedule(10ms, [&] {
                    order.add("first");
                    static_cast<void>(scheduler.schedule(10ms, [&order] { order.add("second"); }));
                }));
            });

        REQUIRE(order.waitFor(2));
    }

    REQUIRE(order.fired() == std::vector<std::string>{"first", "second"});
}
