#ifndef REMOTECONTROL_H
#define REMOTECONTROL_H

#include <functional>
#include <memory>
#include <optional>
#include <boost/asio.hpp>

#include "Logging.h"
#include "TcpServer.h"


namespace remotecontrol {
   
   /**
    * Accepts only one TCP connection at the same time and provides received 
    * commands and connection status updates to the user.
    */
   class RemoteControl : public network::TcpServer::Listener {
      public:
         class Listener {
               public:
                  virtual void onNewConnection()    = 0;
                  virtual void onConnectionClosed() = 0;
                  virtual void onCommandReceived(const std::string& command) = 0;
         };
   
         RemoteControl();
         
         ~RemoteControl();
         
         /**
          * Starts listening for incoming TCP connections and informs the provided
          * listener on changes.
          */
         void start(Listener& remoteControlListener);
         
         /**
          * Asynchronously send the provided message. This method returns before 
          * the message was sent. No additional characters (like CR) get added.
          */
         void asyncSend(const std::string& message, bool appendEndl = true);

         // callbacks of the listener interface of the TcpServer
         void onNewConnection(std::unique_ptr<network::Connection> connection) override;

         void onConnectionClosed() override;
         
         void onCommandReceived(const std::string& command) override;
         
      private:
         class NoopListener : public Listener {
               public:
                  void onNewConnection() override {};
                  void onConnectionClosed() override {};
                  virtual void onCommandReceived(const std::string& /*command*/) override {};
         };
   
         logging::Logger                      log;
         NoopListener                         noopListener;
         std::reference_wrapper<Listener>     listener{noopListener};
         std::unique_ptr<network::TcpServer>  tcpServer;
         std::unique_ptr<network::Connection> connection;
   };
}
#endif
