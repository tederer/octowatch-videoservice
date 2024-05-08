
#include <sstream>

#include "RemoteControl.h"

#define PORT 8889

using logging::Logger;
using network::Connection;
using network::TcpConnection;
using network::TcpServer;
using remotecontrol::RemoteControl;

RemoteControl::RemoteControl() : log("RemoteControl") {}

RemoteControl::~RemoteControl() {
   if (tcpServer) {
      tcpServer->stop();
   }
}

void RemoteControl::start(RemoteControl::Listener& remoteControlListener) {
   listener = remoteControlListener;
   tcpServer.reset(new TcpServer(PORT, "RemoteControl", *this));
   tcpServer->start();
}
         
void RemoteControl::asyncSend(const std::string& message, bool appendEndl) {
   auto conn = connection.get();
   if (!conn) {
      log.info("discarding message because no remote connection exists");
   } else {
      log.info("sending:", message);
      std::ostringstream messageToSend;
      messageToSend << message;
      if (appendEndl) {
         messageToSend << std::endl;
      }
      conn->asyncSend(messageToSend.str());
   }
}

void RemoteControl::onNewConnection(std::unique_ptr<Connection> connection) {
   log.info("accepted new connection");
   this->connection = std::move(connection);
   listener.get().onNewConnection();
}

void RemoteControl::onConnectionClosed() {
   log.info("connection lost");
   listener.get().onConnectionClosed();
   connection.release();
}

void RemoteControl::onCommandReceived(const std::string& command) {
   log.info("received command:", command);
   listener.get().onCommandReceived(command);
}
