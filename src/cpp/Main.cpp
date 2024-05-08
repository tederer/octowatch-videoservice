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
               executor(std::bind(&Impl::onTaskFinished, this, std::placeholders::_1)),
               camera(),
               cameraControl(camera),
               mpjpegConnected(false),
               h264Connected(false),
               systemTemperature(),
               nextTaskId(0),
               currentTaskId(-1) {
               
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
      
      void onTaskFinished(int taskId) {
         log.debug("finished task:", taskId);
         currentTaskId = -1;
      }
      
      int getNextTaskId() {
         int result = nextTaskId;
         nextTaskId = (nextTaskId + 1) % 1000;
         return result;
      }
      
      void onNewFrame(FrameBuffer *highResolutionFrameBuffer, 
                      FrameBuffer *lowResolutionFrameBuffer, int64_t timestamp) {

         log.debug("new frame with timestamp ", timestamp);
         if (currentTaskId >= 0) {
            log.warning("ignoring frame because previous frame is still in progress.");
            return;
         }
         
         if (mpjpegConnected) {
            currentTaskId = getNextTaskId();
            log.debug("enqueuing task", currentTaskId);
            executor.execute([this, lowResolutionFrameBuffer](){
               mpjegStream->send(lowResolutionFrameBuffer);
            }, currentTaskId);
         }
                         
         if (h264Connected) {
            h264Stream->send(highResolutionFrameBuffer, timestamp);
         }
      }

      void systemTemperatureTooHigh(bool tooHigh) {
         log.info("system temperature is", (tooHigh ? "too high -> stopping video streams" : "in allowed range"));
         if (tooHigh) {
            h264Stream.reset();
            mpjegStream.reset();
         } else {
            startVideoStreams();
         }
      }
      
   private:
      Logger                                   log;
      SingleThreadedExecutor                   executor;
      Camera                                   camera;
      CameraControl                            cameraControl;
      bool                                     mpjpegConnected;
      bool                                     h264Connected;
      std::unique_ptr<H264Stream>              h264Stream;
      std::unique_ptr<MultipartJpegHttpStream> mpjegStream;
      SystemTemperature                        systemTemperature;
      int                                      nextTaskId;
      int                                      currentTaskId;
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
