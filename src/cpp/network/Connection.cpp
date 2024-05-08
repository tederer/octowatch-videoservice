
#include "TcpServer.h"

using network::Connection;
using network::TcpConnection;


Connection::Connection(std::unique_ptr<TcpConnection> tcpConnection)
   : tcpConnection(std::move(tcpConnection)) {}

void Connection::asyncSend(const std::string& message) {
   tcpConnection->asyncSend(message);
}

void Connection::asyncSend(void *mem, size_t size) {
   tcpConnection->asyncSend(mem, size);
}

void Connection::asyncSendAndFree(void *mem, size_t size) {
   tcpConnection->asyncSendAndFree(mem, size);
}

bool Connection::outputBufferEmpty() const {
   return tcpConnection->outputBufferEmpty();
}