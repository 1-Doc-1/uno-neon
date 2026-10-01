#include "test_client.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace uno::testing {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
using SocketLength = int; // Winsock takes lengths as int
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

void closeSocket(NativeSocket socket)
{
    closesocket(socket);
}

void ensureSocketsStarted()
{
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA data{};
        static_cast<void>(WSAStartup(MAKEWORD(2, 2), &data));
    });
}

bool receiveTimedOut()
{
    return WSAGetLastError() == WSAETIMEDOUT;
}
#else
using NativeSocket = int;
using SocketLength = std::size_t; // POSIX takes lengths as size_t
constexpr NativeSocket kInvalidSocket = -1;

void closeSocket(NativeSocket socket)
{
    close(socket);
}

void ensureSocketsStarted() {}

bool receiveTimedOut()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}
#endif

using Clock = std::chrono::steady_clock;

enum class ReadResult : std::uint8_t { Data, Timeout, Closed };

// The connection and what was read from it but not consumed yet.
class Connection {
public:
    static std::optional<Connection> open(std::uint16_t port)
    {
        ensureSocketsStarted();
        const NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket == kInvalidSocket) {
            return std::nullopt;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            closeSocket(socket);
            return std::nullopt;
        }
        return Connection(socket);
    }

    Connection(Connection&& other) noexcept
        : socket_(std::exchange(other.socket_, kInvalidSocket)), buffer_(std::move(other.buffer_))
    {
    }

    Connection& operator=(Connection&& other) noexcept
    {
        if (this != &other) {
            release();
            socket_ = std::exchange(other.socket_, kInvalidSocket);
            buffer_ = std::move(other.buffer_);
        }
        return *this;
    }

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    ~Connection() { release(); }

    [[nodiscard]] bool sendAll(std::string_view data) const
    {
        while (!data.empty()) {
            const auto sent =
                ::send(socket_, data.data(),
                       static_cast<SocketLength>(std::min<std::size_t>(data.size(), std::size_t{1} << 20)), 0);
            if (sent <= 0) {
                return false;
            }
            data.remove_prefix(static_cast<std::size_t>(sent));
        }
        return true;
    }

    // Reads more bytes into the buffer, waiting at most until `deadline`.
    ReadResult fill(Clock::time_point deadline)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        if (remaining.count() <= 0) {
            return ReadResult::Timeout;
        }
        setReceiveTimeout(remaining);
        std::array<char, 8192> chunk{};
        const auto received = ::recv(socket_, chunk.data(), static_cast<SocketLength>(chunk.size()), 0);
        if (received > 0) {
            buffer_.append(chunk.data(), static_cast<std::size_t>(received));
            return ReadResult::Data;
        }
        if (received < 0 && receiveTimedOut()) {
            return ReadResult::Timeout;
        }
        return ReadResult::Closed;
    }

    // Waits until the buffer holds at least `count` bytes.
    ReadResult require(std::size_t count, Clock::time_point deadline)
    {
        while (buffer_.size() < count) {
            if (const auto result = fill(deadline); result != ReadResult::Data) {
                return result;
            }
        }
        return ReadResult::Data;
    }

    std::string take(std::size_t count)
    {
        std::string taken = buffer_.substr(0, count);
        buffer_.erase(0, count);
        return taken;
    }

    [[nodiscard]] std::string& buffer() noexcept { return buffer_; }

private:
    explicit Connection(NativeSocket socket) : socket_(socket) {}

    void release() noexcept
    {
        if (socket_ != kInvalidSocket) {
            closeSocket(socket_);
            socket_ = kInvalidSocket;
        }
    }

    void setReceiveTimeout(std::chrono::milliseconds timeout) const
    {
#ifdef _WIN32
        const auto milliseconds = static_cast<DWORD>(timeout.count());
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&milliseconds),
                   sizeof(milliseconds));
#else
        timeval value{};
        value.tv_sec = static_cast<decltype(value.tv_sec)>(timeout.count() / 1000);
        value.tv_usec = static_cast<decltype(value.tv_usec)>((timeout.count() % 1000) * 1000);
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
#endif
    }

    NativeSocket socket_;
    std::string buffer_;
};

// Reads an HTTP response head (up to the empty line) and returns its status code.
std::optional<int> readStatus(Connection& connection, Clock::time_point deadline)
{
    while (!connection.buffer().contains("\r\n\r\n")) {
        if (connection.fill(deadline) != ReadResult::Data) {
            // The server may close right after a short answer: the head may already be complete.
            break;
        }
    }
    const auto headEnd = connection.buffer().find("\r\n\r\n");
    if (headEnd == std::string::npos) {
        return std::nullopt;
    }
    const std::string head = connection.take(headEnd + 4);
    // "HTTP/1.1 101 Switching Protocols"
    const auto firstSpace = head.find(' ');
    if (firstSpace == std::string::npos) {
        return std::nullopt;
    }
    return std::stoi(head.substr(firstSpace + 1, 3));
}

constexpr std::string_view kHandshakeKey = "dGhlIHNhbXBsZSBub25jZQ==";
constexpr std::array<unsigned char, 4> kMask{0x12, 0x34, 0x56, 0x78};

std::string encodeFrame(std::string_view payload, int opcode)
{
    std::string frame;
    frame.push_back(static_cast<char>(0x80 | opcode)); // FIN + opcode
    const auto size = payload.size();
    if (size < 126) {
        frame.push_back(static_cast<char>(0x80 | size));
    } else if (size <= 0xFFFF) {
        frame.push_back(static_cast<char>(0x80 | 126));
        frame.push_back(static_cast<char>(size >> 8));
        frame.push_back(static_cast<char>(size & 0xFF));
    } else {
        frame.push_back(static_cast<char>(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<char>((static_cast<std::uint64_t>(size) >> shift) & 0xFF));
        }
    }
    for (const unsigned char byte : kMask) {
        frame.push_back(static_cast<char>(byte));
    }
    for (std::size_t index = 0; index < size; ++index) {
        frame.push_back(static_cast<char>(static_cast<unsigned char>(payload[index]) ^ kMask[index % kMask.size()]));
    }
    return frame;
}

} // namespace

std::optional<HttpResponse> httpGet(std::uint16_t port, std::string_view path)
{
    auto connection = Connection::open(port);
    if (!connection) {
        return std::nullopt;
    }
    const std::string request =
        "GET " + std::string(path) + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    if (!connection->sendAll(request)) {
        return std::nullopt;
    }
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    // `Connection: close`: the response ends when the server closes.
    while (connection->fill(deadline) == ReadResult::Data) {
    }
    const auto status = readStatus(*connection, deadline);
    if (!status) {
        return std::nullopt;
    }
    return HttpResponse{.status = *status, .body = connection->take(connection->buffer().size())};
}

struct TestWebSocket::Impl {
    explicit Impl(Connection openConnection) : connection(std::move(openConnection)) {}

    Connection connection;
    std::optional<Closure> closure;
};

TestWebSocket::TestWebSocket(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
TestWebSocket::TestWebSocket(TestWebSocket&&) noexcept = default;
TestWebSocket& TestWebSocket::operator=(TestWebSocket&&) noexcept = default;
TestWebSocket::~TestWebSocket() = default;

std::optional<TestWebSocket> TestWebSocket::connect(std::uint16_t port, std::string_view origin, int& status)
{
    auto connection = Connection::open(port);
    if (!connection) {
        return std::nullopt;
    }
    std::string request = "GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: " +
                          std::string(kHandshakeKey) + "\r\n";
    if (!origin.empty()) {
        request += "Origin: " + std::string(origin) + "\r\n";
    }
    request += "\r\n";
    if (!connection->sendAll(request)) {
        return std::nullopt;
    }
    const auto answer = readStatus(*connection, Clock::now() + std::chrono::seconds(5));
    if (!answer) {
        return std::nullopt;
    }
    status = *answer;
    return TestWebSocket(std::make_unique<Impl>(std::move(*connection)));
}

bool TestWebSocket::send(std::string_view payload, int opcode)
{
    return impl_->connection.sendAll(encodeFrame(payload, opcode));
}

std::optional<Frame> TestWebSocket::receiveFrame(std::chrono::steady_clock::time_point deadline)
{
    Connection& connection = impl_->connection;
    // `need` waits for bytes; the end of the connection is remembered as a closure without a close frame.
    const auto need = [&](std::size_t count) {
        const auto result = connection.require(count, deadline);
        if (result == ReadResult::Closed && !impl_->closure) {
            impl_->closure = Closure{};
        }
        return result == ReadResult::Data;
    };

    if (!need(2)) {
        return std::nullopt;
    }
    const auto lengthByte = static_cast<unsigned char>(connection.buffer().at(1)) & 0x7FU;
    std::size_t headerSize = 2;
    if (lengthByte == 126) {
        headerSize = 4;
    } else if (lengthByte == 127) {
        headerSize = 10;
    }
    if (!need(headerSize)) {
        return std::nullopt;
    }
    std::uint64_t length = lengthByte;
    if (headerSize > 2) {
        length = 0;
        for (const char byte : connection.buffer().substr(2, headerSize - 2)) {
            length = (length << 8U) | static_cast<unsigned char>(byte);
        }
    }
    if (!need(headerSize + length)) {
        return std::nullopt;
    }
    const int opcode = static_cast<unsigned char>(connection.buffer().at(0)) & 0x0F;
    static_cast<void>(connection.take(headerSize));
    return Frame{.opcode = opcode, .payload = connection.take(static_cast<std::size_t>(length))};
}

std::optional<std::string> TestWebSocket::receiveText(std::chrono::milliseconds timeout)
{
    constexpr int kText = 0x1;
    constexpr int kClose = 0x8;
    constexpr int kPing = 0x9;
    constexpr int kPong = 0xA;

    const auto deadline = Clock::now() + timeout;
    while (!impl_->closure) {
        auto frame = receiveFrame(deadline);
        if (!frame) {
            return std::nullopt;
        }
        if (frame->opcode == kText) {
            return std::move(frame->payload);
        }
        if (frame->opcode == kPing) {
            static_cast<void>(send(frame->payload, kPong));
        } else if (frame->opcode == kClose) {
            Closure closure;
            if (frame->payload.size() >= 2) {
                closure.code = (static_cast<unsigned char>(frame->payload.at(0)) << 8) |
                               static_cast<unsigned char>(frame->payload.at(1));
                closure.reason = frame->payload.substr(2);
            }
            impl_->closure = std::move(closure);
        }
    }
    return std::nullopt;
}

std::optional<Closure> TestWebSocket::waitForClosure(std::chrono::milliseconds timeout)
{
    const auto deadline = Clock::now() + timeout;
    while (!impl_->closure && Clock::now() < deadline) {
        static_cast<void>(receiveText(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now())));
    }
    return impl_->closure;
}

} // namespace uno::testing
