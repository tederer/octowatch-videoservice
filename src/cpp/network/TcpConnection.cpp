
#include <cstdlib>
#include <cstring>

#include "TcpServer.h"

using network::Connection;
using network::TcpConnection;
using network::TcpServer;
using logging::Logger;

std::unique_ptr<TcpConnection> TcpConnection::create(
   boost::asio::io_context& ioContext, Logger& log) {
   return std::unique_ptr<TcpConnection>(new TcpConnection(ioContext, log));
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

TcpConnection::TcpConnection( boost::asio::io_context& ioContext, Logger& log) 
   :  log(log), 
      pendingOutputByteCount(0), 
      aborted(false),
      socket(ioContext),
      readBuffer(),      
      writeBuffer(boost::asio::const_buffer())
      {}

TcpConnection::~TcpConnection() {
   if (!aborted && socket.is_open()) {
      socket.close();
   }
}

void TcpConnection::sendQueuedData() {
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
   std::size_t charCount   = message.size();
   std::size_t sizeInBytes = charCount * sizeof(char);
   void* messageCopy       = std::malloc(sizeInBytes);
   
   std::memcpy(messageCopy, (void*)message.c_str(), sizeInBytes);
   auto buffer = boost::asio::const_buffer(std::move(boost::asio::buffer(messageCopy, charCount)));
   send(buffer);
}   

void TcpConnection::asyncSend(void *mem, size_t size) {
   void* dataCopy = std::malloc(size);
   std::memcpy(dataCopy, mem, size);
   
   auto buffer = boost::asio::const_buffer(std::move(boost::asio::buffer(dataCopy, size)));
   send(buffer);
}

void TcpConnection::asyncSendAndFree(void *mem, size_t size) {
   auto buffer = boost::asio::const_buffer(std::move(boost::asio::buffer(mem, size)));
   send(buffer);
}

bool TcpConnection::outputBufferEmpty() {
   const std::lock_guard<std::mutex> lock(mutex);
   return ((sendQueue.size() <= 0) && (pendingOutputByteCount <= 0));
}

void TcpConnection::close() {
   socket.close();
   connectionClosedCallback();
}

void TcpConnection::onWriteComplete(const boost::system::error_code& error, 
                                    size_t bytes_transferred) {
   if (error) {
      log.error("failed to write:", error.message());
      close();
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

