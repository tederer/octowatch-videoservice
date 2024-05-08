
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>
#include <sys/mman.h>
#include <thread>

#include "MultipartJpegHttpStream.h"

#define PORT 8887
#define CRLF "\r\n"

#define WIDTH              800
#define HEIGHT             600
#define DEFAULT_QUALITY    95
#define RESTART_INTERVAL   0

using libcamera::StreamConfiguration;
using logging::Logger;
using network::Connection;
using network::TcpServer;

using namespace std::chrono_literals;

MultipartJpegHttpStream::MultipartJpegHttpStream(StreamConfiguration const &streamConfig,
                                                 ConnectedCallback callback) 
   : log("MultipartJpegHttpStream"), 
     connectedCallback(callback) {
        
   char* qualityEnvVar = std::getenv("OCTOWATCH_JPEG_QUALITY");
   
   int quality = DEFAULT_QUALITY;
     
   if (qualityEnvVar != nullptr) {
      std::string inputText(qualityEnvVar);
      const std::regex regex("\\s*(\\d+)\\s*");
      std::smatch captureGroups;
      if (std::regex_match(inputText, captureGroups, regex)) {
         if (captureGroups.size() >= 2) {
            std::string qualityAsString = captureGroups[1].str();
            int qualityToUse = std::stoi(qualityAsString);
            if ((qualityToUse >= 0) && (qualityToUse <= 100)) {
               quality = qualityToUse;
            } else {
               log.warning("ignoring quality requested via env var because it's out of range [0,100].");
            }
         }
      } else {
         log.warning("ignoring quality requested via env var because it's not an integer.");
      }
   }
   log.info("jpeg quality =", quality);
   jpegEncoder.reset(new JpegEncoder(streamConfig, quality));
}

MultipartJpegHttpStream::~MultipartJpegHttpStream() {
   connectedCallback(false);
   if (tcpServer) {
      tcpServer->stop();
   }
}
 
void MultipartJpegHttpStream::start() {
   connectedCallback(false);
   tcpServer.reset(new TcpServer(PORT, "MPJPEG", *this));
   tcpServer->start();
}

void MultipartJpegHttpStream::send(libcamera::FrameBuffer *frameBuffer) {
            
   auto firstPlane          = frameBuffer->planes()[0];
   void* frameBufferContent = mmap(nullptr, firstPlane.length, PROT_READ, MAP_SHARED,
                                   firstPlane.fd.get(), firstPlane.offset);
   
   if (frameBufferContent != MAP_FAILED) {
      auto start = std::chrono::steady_clock::now();
      JpegEncoder::JpegImage image = jpegEncoder->encode((uint8_t*)frameBufferContent + firstPlane.offset, firstPlane.length);
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> diff = end - start;
      log.debug("JPEG encoding duration =", (int)(diff.count() * 1000), "ms"); 
      if (image.size > 0 && image.data != nullptr) {
         sendJpeg(image.data, image.size);
      }
      if (munmap(frameBufferContent, firstPlane.length) != 0) {
         log.error("failed to unmap DMA buffer");
      }  
   } else {
      log.error("failed to map DMA buffer: error", errno);
   }
}

void MultipartJpegHttpStream::sendJpeg(void *data, size_t size) {
   std::ostringstream messageToSend;
   messageToSend << "--FRAME" << CRLF;
   messageToSend << "Content-Type: image/jpeg" << CRLF;
   messageToSend << "Content-Length: " << size << CRLF << CRLF;   
   {
      const std::lock_guard<std::mutex> lock(connectionMutex);
      if (connection != nullptr) {
         connection->asyncSend(messageToSend.str()); 
         connection->asyncSendAndFree(data, size);   
         connection->asyncSend(std::string(CRLF).append(CRLF));
         
         while (!connection->outputBufferEmpty()) {
            std::this_thread::sleep_for(5ms); // TODO are we losing time here?
         }
      }
   }
}

void MultipartJpegHttpStream::onNewConnection(std::unique_ptr<Connection> conn) {
   log.info("accepted new connection");
   {
      const std::lock_guard<std::mutex> lock(connectionMutex);
      connection = std::move(conn);
   }
   connectedCallback(true);
}

void MultipartJpegHttpStream::onConnectionClosed() {
   log.info("connection lost");
   {
      const std::lock_guard<std::mutex> lock(connectionMutex);
      connection.reset();
   }
   connectedCallback(false);
}

void MultipartJpegHttpStream::onCommandReceived(const std::string& command) {
   if (command == std::string("\r")) {
      log.info("received new HTTP request -> starting to send multipart response");
      std::ostringstream messageToSend;
      messageToSend << "HTTP/1.1 200 OK" << CRLF;
      messageToSend << "Content-Type: multipart/x-mixed-replace;boundary=FRAME" << CRLF << CRLF;
      {
         const std::lock_guard<std::mutex> lock(connectionMutex);
         if (connection != nullptr) {
            connection->asyncSend(messageToSend.str());
         }
      }
   }
}
