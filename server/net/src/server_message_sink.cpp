#include "uno/net/server_message_sink.hpp"

#include "uno/net/codec.hpp"

namespace uno::net {

void ServerMessageSink::send(app::ConnectionId connection, const app::response::Message& message)
{
    server_->send(connection, encodeServerMessage(message));
}

void ServerMessageSink::close(app::ConnectionId connection, int code, std::string_view reason)
{
    server_->close(connection, code, reason);
}

} // namespace uno::net
