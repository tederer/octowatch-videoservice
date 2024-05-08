#ifndef MULTIPARTJPEGHTTPSTREAM_H
#define MULTIPARTJPEGHTTPSTREAM_H

#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <mutex>

#include "libcamera/stream.h"

#include "JpegEncoder.h"
#include "Logging.h"
#include "TcpServer.h"

#if JPEG_LIB_VERSION_MAJOR > 9 || (JPEG_LIB_VERSION_MAJOR == 9 && JPEG_LIB_VERSION_MINOR >= 4)
typedef size_t jpeg_mem_len_t;
#else
typedef unsigned long jpeg_mem_len_t;
#endif

typedef std::function<void(bool)> ConnectedCallback;

/**
 * This class converts the provided frame to an JPG image and sends it
 * as a HTTP multipart stream (RFC1341).
 *
 * https://www.w3.org/Protocols/rfc1341/7_2_Multipart.html
 * https://www.codeinsideout.com/blog/pi/stream-picamera-mjpeg/
 */
class MultipartJpegHttpStream : network::TcpServer::Listener {
   public:
      MultipartJpegHttpStream(libcamera::StreamConfiguration const &streamConfig, 
                              ConnectedCallback callback);
      
      ~MultipartJpegHttpStream();
      
      void start();
      
      void send(libcamera::FrameBuffer *frameBuffer);
      
      // callbacks of the listener interface of the TcpServer
      void onNewConnection(std::unique_ptr<network::Connection> connection) override;

      void onConnectionClosed() override;
      
      void onCommandReceived(const std::string& command) override;
      
   private:
      void sendJpeg(void *data, size_t size);
      
      logging::Logger                      log;
      std::unique_ptr<JpegEncoder>         jpegEncoder;
      std::unique_ptr<network::TcpServer>  tcpServer;
      std::unique_ptr<network::Connection> connection;
      std::mutex                           connectionMutex;
      ConnectedCallback                    connectedCallback;
};
#endif
