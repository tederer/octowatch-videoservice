#include <chrono>
#include <cstdlib>
#include <iostream>
#include <errno.h>
#include <functional>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <thread>

#include "libcamera/framebuffer.h"

#include "Camera.h"
#include "CameraControl.h"
#include "H264Stream.h"
#include "Logging.h"
#include "MultipartJpegHttpStream.h"
#include "SingleThreadedExecutor.h"
#include "SystemTemperature.h"

using libcamera::FrameBuffer;
using logging::Logger;

using namespace std::chrono_literals;

class Impl {
   public:
      Impl() : log("Impl"),
               camera(),
               cameraControl(camera),
               mpjpegConnected(false),
               h264Connected(false),
               systemTemperature() {
               
         startVideoStreams();
         cameraControl.start();
         systemTemperature.start(std::bind(&Impl::systemTemperatureTooHigh, this, std::placeholders::_1));
      }
      
      ~Impl() { 
         camera.stop();
      }
      
      void startVideoStreams() {
         if (!h264Stream) {
            h264Stream.reset(new H264Stream(camera.getStreamConfiguration(StreamType::HIGH_RESOLUTION), 
                          std::bind(&Impl::onH264Connected, this, std::placeholders::_1)));
            h264Stream->start();
         }
         if (!mpjegStream) {
            mpjegStream.reset(new MultipartJpegHttpStream(
                          camera.getStreamConfiguration(StreamType::LOW_RESOLUTION), 
                          std::bind(&Impl::onMpjpegConnected, this, std::placeholders::_1)));
            mpjegStream->start();
         }
      }
      
      void stopVideoStreams() {
         h264Stream.reset();
         mpjegStream.reset();
      }
      
      void updateCameraState() {
         if (!mpjpegConnected && !h264Connected) {
            camera.stop();
            return;
         }
         
         if (!camera.isStarted() && (mpjegStream || h264Stream)) {
            auto frameConsumer = std::bind(&Impl::onNewFrame, this, std::placeholders::_1, 
                                           std::placeholders::_2, std::placeholders::_3);
            if(!camera.start(frameConsumer)) {
               log.error("failed to start camera");
            }
         }
      }
      
      void onMpjpegConnected(bool connected) {
         mpjpegConnected = connected;
         updateCameraState();
      }
      
      void onH264Connected(bool connected) {
         h264Connected = connected;
         updateCameraState();
      }
      
      void onNewFrame(FrameBuffer *highResolutionFrameBuffer, 
                      FrameBuffer *lowResolutionFrameBuffer, int64_t timestamp) {

         log.debug("new frame with timestamp ", timestamp);
                         
         if (h264Connected) {
            h264Stream->send(highResolutionFrameBuffer, timestamp);
         }
         
         if (mpjpegConnected) {
            mpjegStream->send(lowResolutionFrameBuffer, timestamp);
         }
      }

      void systemTemperatureTooHigh(bool tooHigh) {
         log.info("system temperature is", (tooHigh ? "too high -> stopping video streams" : "in allowed range"));
         if (tooHigh) {
            stopVideoStreams();
         } else {
            startVideoStreams();
         }
      }
      
   private:
      Logger                                   log;
      Camera                                   camera;
      CameraControl                            cameraControl;
      bool                                     mpjpegConnected;
      bool                                     h264Connected;
      std::unique_ptr<H264Stream>              h264Stream;
      std::unique_ptr<MultipartJpegHttpStream> mpjegStream;
      SystemTemperature                        systemTemperature;
};

int main() {
   char* logLevel = std::getenv("OCTOWATCH_LOG_LEVEL");
   
   logging::minLevel = INFO;
   
   if (logLevel != nullptr) {
      for(auto const &tuple : logging::LOG_LEVEL_NAME) {
         if (tuple.second == logLevel) {
            logging::minLevel = tuple.first;
            break;
         }
      }
   }
   
   Logger log("Main");
   log.info("log level:", logging::LOG_LEVEL_NAME[logging::minLevel]);
   
   Impl impl;
   
   while(true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
   }      
   return 0;
}
