#pragma once

// A deliberately tiny blocking HTTP and WebSocket client over raw sockets, for the tests of the real
// server (SPEC §17). It does just enough of RFC 6455 to open a connection, send masked frames of any
// opcode and read what comes back, so the tests can also send what a well-behaved library never would
// (oversized, binary or malformed frames). No third-party dependency: see ADR 0014.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace uno::testing {

struct HttpResponse {
    int status = 0;
    std::string body;
};

// One GET request on a fresh connection.
[[nodiscard]] std::optional<HttpResponse> httpGet(std::uint16_t port, std::string_view path);

struct Frame {
    int opcode = 0;
    std::string payload;
};

// What ended a WebSocket: the close code the server sent, if it sent one.
struct Closure {
    std::optional<int> code;
    std::string reason;
};

class TestWebSocket {
public:
    // Opens the TCP connection and performs the HTTP upgrade. `origin` empty = no Origin header.
    // `status` is the HTTP status of the answer (101 when the upgrade succeeded); the object is only
    // returned usable for frames when it is 101.
    [[nodiscard]] static std::optional<TestWebSocket> connect(std::uint16_t port, std::string_view origin, int& status);

    TestWebSocket(TestWebSocket&&) noexcept;
    TestWebSocket& operator=(TestWebSocket&&) noexcept;
    TestWebSocket(const TestWebSocket&) = delete;
    TestWebSocket& operator=(const TestWebSocket&) = delete;
    ~TestWebSocket();

    // Sends one masked frame (opcode 0x1 text, 0x2 binary...).
    [[nodiscard]] bool send(std::string_view payload, int opcode = 0x1);

    // Next text frame, answering pings on the way. Empty on timeout, or once the server closed
    // (see closure()).
    [[nodiscard]] std::optional<std::string> receiveText(std::chrono::milliseconds timeout = std::chrono::seconds(5));

    // Waits until the server closes the connection (or the timeout), skipping any frame before it.
    [[nodiscard]] std::optional<Closure> waitForClosure(std::chrono::milliseconds timeout = std::chrono::seconds(5));

private:
    struct Impl;
    [[nodiscard]] std::optional<Frame> receiveFrame(std::chrono::steady_clock::time_point deadline);
    explicit TestWebSocket(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace uno::testing
