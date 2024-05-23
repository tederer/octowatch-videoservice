#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <functional>
#include <memory>
#include <mutex>
#include <queue>

#include <boost/asio.hpp>

#include "Logging.h"

namespace network {
   
   class TcpConnection {
      public:
         static std::unique_ptr<TcpConnection> create(boost::asio::io_context& io_context, 
                                                      const std::string& name);

         ~TcpConnection();
         
         boost::asio::ip::tcp::socket& getSocket();

         void start( std::function<void()> connectionClosedCallback, 
                     std::function<void(const std::string&)> commandConsumer);
         
         /**
          * A copy of the message gets send asynchronuously. The caller can
          * destroy the message as soon as asyncSend returned.
          **/
         void asyncSend(const std::string& message);
         
         /**
          * The provided memory can get freed as soon as asyncSend returned.
          * Internally a copy of mem gets created for async processing.
          **/
         void asyncSend(void *mem, size_t size);
         
         /**
          * The provided memory gets freed by this method after sending.
          * No internal copy gets used.
          **/
         void asyncSendAndFree(void *mem, size_t size);
         
         bool outputBufferEmpty();
         
      private:
         TcpConnection( boost::asio::io_context& io_context, const std::string& name);

         void readNextLine();
         
         void send(boost::asio::const_buffer& buffer);

         void close();
         
         void sendQueuedData();
         
         void onWriteComplete(const boost::system::error_code& error, size_t bytes_transferred);
         
         void onReadLineComplete(const boost::system::error_code& error, size_t bytes_transferred);

         logging::Logger                        log;
         boost::asio::io_context&               ioContext;
         size_t                                 pendingOutputByteCount;
         bool                                   aborted;
         bool                                   closed;
         boost::asio::ip::tcp::socket           socket;
         boost::asio::streambuf                 readBuffer;
         boost::asio::const_buffer              writeBuffer;
         std::function<void()>                  connectionClosedCallback;
         std::function<void(std::string&)>      commandConsumer;
         std::queue<boost::asio::const_buffer>  sendQueue;
         std::mutex                             mutex;
   };

   class Connection {
      public:
         Connection(std::unique_ptr<TcpConnection> tcpConnection);
         
         void asyncSend(const std::string& message);

         void asyncSend(void *mem, size_t size);
         
         void asyncSendAndFree(void *mem, size_t size);
         
         bool outputBufferEmpty() const;
         
      private:
         std::unique_ptr<TcpConnection> tcpConnection;
   };   

   class TcpServer {
      public:
         class Listener {
            public:
               virtual void onNewConnection(std::unique_ptr<Connection> connection) = 0;
               virtual void onConnectionClosed() = 0;
               virtual void onCommandReceived(const std::string& command)  = 0;
         };

         TcpServer(unsigned int port, const std::string& name, Listener& tcpServerListener);

         ~TcpServer();
         
         void start();
         
         void stop();
         
      private:
         void onConnectionClosed();

         void onCommandReceived(const std::string& command);

         void handle_accept(std::unique_ptr<TcpConnection> newConnection, 
                              const boost::system::error_code& error);

         logging::Logger                                 log;
         std::string                                     name;
         unsigned int                                    port;
         bool                                            stopped;
         Listener&                                       listener;
         std::unique_ptr<boost::asio::io_context>        ioContext;
         std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
         std::unique_ptr<std::thread>                    thread;
   };
}
#endif
