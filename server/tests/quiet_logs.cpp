#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <spdlog/spdlog.h>

namespace {

// The application logs every room and player it creates: useful in production, noise in a test run.
class QuietLogs : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo& /*info*/) override { spdlog::set_level(spdlog::level::warn); }
};

} // namespace

CATCH_REGISTER_LISTENER(QuietLogs)
