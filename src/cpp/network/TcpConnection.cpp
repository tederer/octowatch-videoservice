#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "TcpServer.h"

using network::Connection;
using network::TcpConnection;
using network::TcpServer;
using logging::Logger;

using namespace std::chrono_literals;

std::unique_ptr<TcpConnection> TcpConnection::create(
   boost::asio::io_context& ioContext, const std::string& name) {
   return std::unique_ptr<TcpConnection>(new TcpConnection(ioContext, name));
}

boost::asio::ip::tcp::socket& TcpConnection::getSocket() {
   return socket;
}

void TcpConnection::start( std::function<void()> connectionClosedCallback, 
                           std::function<void(const std::string&)> commandConsumer) {
   this->connectionClosedCallback = connectionClosedCallback;
   this->commandConsumer          = commandConsumer;
   readNextLine();
}

TcpConnection::TcpConnection( boost::asio::io_context& ioContext, const std::string& name) 
   :  log((std::string("TcpConnection-").append(name)).c_str()), 
      ioContext(ioContext),
      pendingOutputByteCount(0), 
      aborted(false),
      closed(false),
      socket(ioContext),
      readBuffer(),      
      writeBuffer(boost::asio::const_buffer())
      {}

TcpConnection::~TcpConnection() {
   close();
}

void TcpConnection::sendQueuedData() {
   if (closed) {
      return;
   }
   
   size_t byteCountToSend = 0;
   {
      const std::lock_guard<std::mutex> lock(mutex);
      if (pendingOutputByteCount > 0) {
         return;     // waiting for invocation of onWriteComplete
      } else {
         if (writeBuffer.size() > 0) {
            std::free((void*)writeBuffer.data()); // necessary because the buffer does not own the data
            writeBuffer = boost::asio::const_buffer();
         }
      }
      
      if (sendQueue.empty()) {
         return;
      }
      
      auto front = sendQueue.front();
      writeBuffer = std::move(front);
      sendQueue.pop();
      byteCountToSend = writeBuffer.size();
      pendingOutputByteCount += byteCountToSend;
   }
   log.debug("enqueuing", byteCountToSend, "bytes for sending -> pending byte count:", pendingOutputByteCount);
   boost::asio::async_write(socket, writeBuffer, std::bind(&TcpConnection::onWriteComplete, 
         this, std::placeholders::_1, std::placeholders::_2));
}

void TcpConnection::send(boost::asio::const_buffer& buffer) {
   {
      const std::lock_guard<std::mutex> lock(mutex);
      sendQueue.push(std::move(buffer));
   }
   sendQueuedData();
}

void TcpConnection::asyncSend(const std::string& message) {
   if (closed) {
      return;
   }
   
   std::size_t charCount   = message.size();
   std::size_t sizeInBytes = charCount * sizeof(char);
   void* messageCopy       = std::malloc(sizeInBytes);
   
   std::memcpy(messageCopy, (void*)message.c_str(), sizeInBytes);
   auto buffer = boost::asio::const_buffer(std::move(boost::asio::buffer(messageCopy, charCount)));
   send(buffer);
}   

void TcpConnection::asyncSend(void *mem, size_t size) {
   if (closed) {
      return;
   }
   
   void* dataCopy = std::malloc(size);
   std::memcpy(dataCopy, mem, size);
   
   auto buffer = boost::asio::const_buffer(std::move(boost::asio::buffer(dataCopy, size)));
   send(buffer);
}

void TcpConnection::asyncSendAndFree(void *mem, size_t size) {
   if (closed) {
      return;
   }
   
   auto buffer = boost::asio::const_buffer(std::move(boost::asio::buffer(mem, size)));
   send(buffer);
}

bool TcpConnection::outputBufferEmpty() {
   const std::lock_guard<std::mutex> lock(mutex);
   return ((sendQueue.size() <= 0) && (pendingOutputByteCount <= 0));
}

void TcpConnection::close() {
   if (closed) {
      return;
   }
   
   closed = true;
   if (socket.is_open()) {
      log.info("closing socket");
      socket.close();
   }
   if (!ioContext.stopped()) {
      log.info("stopping ioContext");
      ioContext.stop();
      log.info("waiting for boost::asio::io_context to stop");
      int repetitions = 30;
      while((repetitions > 0) && !ioContext.stopped()) {
         std::this_thread::sleep_for(100ms);
         repetitions--;
      }
      if (ioContext.stopped()) {
         log.info("boost::asio::io_context stopped");
      } else {
         log.error("waiting for termination of boost::asio::io_context timed out");
      }
   }
   connectionClosedCallback();
}

void TcpConnection::onWriteComplete(const boost::system::error_code& error, 
                                    size_t bytes_transferred) {
   if (error) {
      log.error("failed to write:", error.message());
      close();
      return;
   }
   
   bool allBytesSent = false;
   
   {
      const std::lock_guard<std::mutex> lock(mutex);
      pendingOutputByteCount = std::max((size_t)0, pendingOutputByteCount - bytes_transferred);
      allBytesSent           = pendingOutputByteCount == 0;
   }
   
   log.debug("bytes transferred:", bytes_transferred, ", pending bytes:", pendingOutputByteCount);
   
   if (allBytesSent) {
      sendQueuedData();
   }
}

void TcpConnection::readNextLine() {
   log.debug("starting to read next line");
   boost::asio::async_read_until(socket, readBuffer, "\n",
      std::bind(&TcpConnection::onReadLineComplete, this, 
                  std::placeholders::_1, std::placeholders::_2));
}      
  
void TcpConnection::onReadLineComplete(const boost::system::error_code& error, 
                                       size_t bytes_transferred) {
   if (error == boost::asio::error::operation_aborted) {
      log.info("operation aborted");
      aborted = true;
      return;
   }
   
   if (error) {
      log.error("failed to read:", error.message());
      close();
   } else {
      std::istream is(&readBuffer);
      char* line = new char[bytes_transferred];
      is.read(line, bytes_transferred);
      line[bytes_transferred - 1] = 0x00;
      std::string command = line;
      log.debug("received", bytes_transferred,"bytes:", command);
      commandConsumer(command);
      readNextLine();
   }
}

