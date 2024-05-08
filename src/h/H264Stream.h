#ifndef H264STREAM_H
#define H264STREAM_H

#include "libcamera/stream.h"

#include "H264Encoder.h"
#include "Logging.h"
#include "TcpServer.h"

typedef std::function<void(bool)> ConnectedCallback;

/**
 * This class sends the provided frame as a H.264 stream.
 */
class H264Stream : network::TcpServer::Listener {
   public:
      H264Stream(libcamera::StreamConfiguration const &streamConfig, ConnectedCallback callback);
      
      ~H264Stream();
      
      void start();
      
      void send(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us);
      
      // callbacks of the listener interface of the TcpServer
      void onNewConnection(std::unique_ptr<network::Connection> connection) override;

      void onConnectionClosed() override;
      
      void onCommandReceived(const std::string& command) override;
      
   private:
      void onEncoderOutputReady(void *mem, size_t size, int64_t timestamp_us, bool keyframe);
      
      logging::Logger                      log;
      std::unique_ptr<network::TcpServer>  tcpServer;
      std::unique_ptr<network::Connection> connection;
      std::mutex                           connectionMutex;
      H264Encoder                          h264encoder;
      ConnectedCallback                    connectedCallback;
};
#endif
