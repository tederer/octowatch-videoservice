
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

#include "TcpServer.h"

using network::Connection;
using network::TcpConnection;
using network::TcpServer;
using logging::Logger;

using namespace std::chrono_literals;

TcpServer::TcpServer(unsigned int port, const std::string& name, TcpServer::Listener& tcpServerListener)
 : log((std::string("TcpServer-").append(name)).c_str()),
   port(port), 
   stopped(false),
   listener(tcpServerListener) {}

TcpServer::~TcpServer() {
   stop();
   if (thread) {
      log.info("waiting for thread to finish its execution");
      thread->join();
      log.info("thread finished");
   }
}

void TcpServer::start() {
   ioContext.reset(new boost::asio::io_context());
   acceptor.reset(new boost::asio::ip::tcp::acceptor(*ioContext, 
                           boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))); 
   
   stopped = false;
   
   thread = std::unique_ptr<std::thread>(new std::thread([this](){
      while(!stopped) {
         std::unique_ptr<TcpConnection> newConnection = TcpConnection::create(*ioContext, log);
         auto callback = [this, &newConnection](const boost::system::error_code& error){
            handle_accept(std::move(newConnection), error);
         };
         log.info("listening on port", port);
         acceptor->async_accept(newConnection->getSocket(), callback);
         log.info("running ioContext");
         ioContext->run();
         log.info("finished running ioContext");
         if (!stopped) {
            log.info("restarting ioContext");
            ioContext->restart();
         }
      }
      log.info("left loop for accepting incoming connections");
   }));
}

void TcpServer::stop() {
   if (stopped) {
      return;
   }
   
   stopped = true;
   log.info("stopping");
   
   if (acceptor) {
      log.info("canceling boost::asio::acceptor");
      acceptor->cancel();
      log.info("closing boost::asio::acceptor");
      acceptor->close();
   }
   
   if (ioContext) {
      log.info("stopping boost::asio::io_context");
      ioContext->stop();
      log.info("waiting for boost::asio::io_context to stop");
      int repetitions = 30;
      while((repetitions > 0) && !ioContext->stopped()) {
         std::this_thread::sleep_for(100ms);
         repetitions--;
      }
      if (ioContext->stopped()) {
         log.info("boost::asio::io_context stopped");
      } else {
         log.error("waiting for termination of boost::asio::io_context timed out");
      }
   }
}

void TcpServer::onConnectionClosed() {
   listener.onConnectionClosed();
}

void TcpServer::onCommandReceived(const std::string& command) {
   listener.onCommandReceived(command);
}

void TcpServer::handle_accept(std::unique_ptr<TcpConnection> newConnection, 
                              const boost::system::error_code& error) {
   if (stopped) {
      return;
   }
   
   if (error) {
      if (error == boost::asio::error::operation_aborted) {
         log.info("aborted async accept");
      } else {
         log.error("failed to accept connection:", error.message());
      }
   } else {
      newConnection->start(
         std::bind(&TcpServer::onConnectionClosed, this),
         std::bind(&TcpServer::onCommandReceived, this, std::placeholders::_1));
      std::unique_ptr<Connection> connection(new Connection(std::move(newConnection)));
      listener.onNewConnection(std::move(connection));
   }
}
