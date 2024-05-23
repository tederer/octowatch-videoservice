
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>
#include <thread>

#include "CpuJpegEncoder.h"
#include "HardwareJpegEncoder.h"
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
   
   char* jpegEncoderEnvVar = std::getenv("OCTOWATCH_JPEG_ENCODER");
   if (jpegEncoderEnvVar && (strcmp(jpegEncoderEnvVar, "CPU") == 0)) {
      jpegEncoder.reset(new CpuJpegEncoder(streamConfig, quality));
   } else {
      jpegEncoder.reset(new HardwareJpegEncoder(streamConfig, quality));
   }
   
   jpegEncoder->setOutputReadyCallback(std::bind(&MultipartJpegHttpStream::onJpegAvailable, this, 
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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

void MultipartJpegHttpStream::send(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us) {
            
   jpegEncoder->encode(frameBuffer, timestamp_us);
}

void MultipartJpegHttpStream::onJpegAvailable(void *data, size_t bytesCount, int64_t timestamp) {
   sendJpeg(data, bytesCount);
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
         connection->asyncSend(data, size);   
         connection->asyncSend(std::string(CRLF).append(CRLF));
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
