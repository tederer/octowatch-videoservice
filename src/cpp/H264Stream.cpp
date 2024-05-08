#include <functional>

#include "H264Stream.h"

#define PORT 8888

using libcamera::StreamConfiguration;
using logging::Logger;
using network::Connection;
using network::TcpServer;

H264Stream::H264Stream(StreamConfiguration const &streamConfig, ConnectedCallback callback) 
   : log("H264Stream"), 
     h264encoder(streamConfig),
     connectedCallback(callback) {
        
   h264encoder.setOutputReadyCallback(std::bind(&H264Stream::onEncoderOutputReady, this, 
                                                std::placeholders::_1,
                                                std::placeholders::_2,
                                                std::placeholders::_3,
                                                std::placeholders::_4));
}

H264Stream::~H264Stream() {
   connectedCallback(false);
   if (tcpServer) {
      tcpServer->stop();
   }
}
 
void H264Stream::start() {
   connectedCallback(false);
   tcpServer.reset(new TcpServer(PORT, "H.264", *this));
   tcpServer->start();
}

void H264Stream::send(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us) {
   if (connection) {
      h264encoder.encode(frameBuffer, timestamp_us);
   }
}

void H264Stream::onNewConnection(std::unique_ptr<Connection> conn) {
   log.info("accepted new connection");
   {
      const std::lock_guard<std::mutex> lock(connectionMutex);
      connection = std::move(conn);
   }
   connectedCallback(true);
}

void H264Stream::onConnectionClosed() {
   log.info("connection lost");
   {
      const std::lock_guard<std::mutex> lock(connectionMutex);
      connection.reset();
   }
   connectedCallback(false);
}

void H264Stream::onEncoderOutputReady(void *mem, size_t size, int64_t timestamp_us, bool keyframe) {
   if (connection) {
      log.debug("output ready: size =", size, ", timestamp_us = ", timestamp_us);
      connection->asyncSend(mem, size);
   }
}

void H264Stream::onCommandReceived(const std::string& command) {}
