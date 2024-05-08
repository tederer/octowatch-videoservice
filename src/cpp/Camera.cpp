#include <sstream>
#include <thread>

#include <linux/dma-buf.h>

#include "libcamera/control_ids.h"
#include "libcamera/base/unique_fd.h"
#include "libcamera/formats.h"
#include "libcamera/stream.h"

#include "Camera.h"
#include "Logging.h"

#define REQUEST_COUNT   3

using capabilities::CameraCapabilities;
using libcamera::CameraConfiguration;
using libcamera::CameraManager;
using libcamera::ControlList;
using libcamera::FrameBuffer;
using libcamera::Request;
using libcamera::UniqueFD;
using libcamera::Stream;
using logging::Logger;

using namespace std::chrono_literals;

//libcamera::logSetLevel("RPI", "INFO");
//libcamera::logSetLevel("Camera", "INFO"); 

Camera::Camera() 
   :  log("Camera"), 
      started(false), 
      initialized(false),
      pendingRequests(0) {
   initialized = initialize();
}

bool Camera::createRequests(CameraConfiguration *streamConfigs) {
   log.info("creating", REQUEST_COUNT, "request object(s)");
   auto highResolutionFrameSize = streamConfigs->at(HIGH_RESOLUTION).frameSize;
   auto highResolutionStream    = streamConfigs->at(HIGH_RESOLUTION).stream();
   auto lowResolutionFrameSize  = streamConfigs->at(LOW_RESOLUTION).frameSize;
   auto lowResolutionStream     = streamConfigs->at(LOW_RESOLUTION).stream();
   
   log.info("highResolutionFrameSize =", highResolutionFrameSize, "bytes");
   log.info("lowResolutionFrameSize  =", lowResolutionFrameSize,  "bytes");
   
   for (int index = 0; index < REQUEST_COUNT; index++) {
      std::unique_ptr<Request> request = camera->createRequest();
      if (!request) {
         log.error("failed to create request object");
         return false;
      }
      
      std::ostringstream highResolutionName;
      highResolutionName << "highResolution-" << index;
   
      std::ostringstream lowResolutionName;
      lowResolutionName << "lowResolution-" << index;
   
      UniqueFD highResolutionFd = dmaHeap.alloc(highResolutionName.str().c_str(), highResolutionFrameSize);
      UniqueFD lowResolutionFd  = dmaHeap.alloc(lowResolutionName.str().c_str(),  lowResolutionFrameSize);
      
      std::vector<FrameBuffer::Plane> highResolutionPlanes(1);
      highResolutionPlanes[0].fd     = libcamera::SharedFD(std::move(highResolutionFd));
      highResolutionPlanes[0].offset = 0;
      highResolutionPlanes[0].length = highResolutionFrameSize;
      
      std::vector<FrameBuffer::Plane> lowResolutionPlanes(1);
      lowResolutionPlanes[0].fd     = libcamera::SharedFD(std::move(lowResolutionFd));
      lowResolutionPlanes[0].offset = 0;
      lowResolutionPlanes[0].length = lowResolutionFrameSize;
      
      std::unique_ptr<FrameBuffer> highResolutionFrameBuffer(new FrameBuffer(highResolutionPlanes));
      std::unique_ptr<FrameBuffer> lowResolutionFrameBuffer(new FrameBuffer(lowResolutionPlanes));
      request->addBuffer(highResolutionStream, highResolutionFrameBuffer.get());
      request->addBuffer(lowResolutionStream,  lowResolutionFrameBuffer.get());
      frameBuffers.push_back(std::move(highResolutionFrameBuffer));
      frameBuffers.push_back(std::move(lowResolutionFrameBuffer));
      requests.push_back(std::move(request));
   }
   
   return true;
}

bool Camera::initialize() {
   cameraManager.reset(new CameraManager());
   int errorCode = cameraManager->start();
   if (errorCode != 0) {
      log.error("failed to start camera manager: error code", errorCode);
      return false;
   }
   std::vector<std::shared_ptr<libcamera::Camera>> cameras = cameraManager->cameras();
   for (auto camera : cameras) {
      log.info("found camera:", camera->id());
   }
   if (cameras.size() != 1) {
      log.error("expected to find 1 camere but found", cameras.size());
      return false;
   }
   
   camera = cameras[0];
   log.info("acquiring exclusive access");
   errorCode = camera->acquire();
   if (errorCode != 0) {
      log.error("failed to get exclusive access to camera: error code", errorCode);
      return false;
   }
   
   log.info("generating configuration");
   streamConfigs = camera->generateConfiguration({ libcamera::StreamRole::Raw, libcamera::StreamRole::Viewfinder });

   log.info("default config for stream raw and viewfinder stream role:");
   for (auto streamConfig : *streamConfigs) {
      log.info("   stream config:", streamConfig.toString());
      for (auto pixelFormat : streamConfig.formats().pixelformats()) {
         log.info("      pixel format:", pixelFormat.toString());
      }
   }
	
   if (streamConfigs->size() != 2) {
      log.error("expected 2 configured stream but got", streamConfigs->size());
      return false;
   }
   
   streamConfigs->at(HIGH_RESOLUTION).pixelFormat = libcamera::formats::YUV420;
   streamConfigs->at(HIGH_RESOLUTION).size.width  = 1920;
   streamConfigs->at(HIGH_RESOLUTION).size.height = 1080;
   streamConfigs->at(HIGH_RESOLUTION).colorSpace  = libcamera::ColorSpace::Rec709;
                                            
   streamConfigs->at(LOW_RESOLUTION).pixelFormat  = libcamera::formats::YUV420;
   streamConfigs->at(LOW_RESOLUTION).size.width   = 800;
   streamConfigs->at(LOW_RESOLUTION).size.height  = 600;
   streamConfigs->at(LOW_RESOLUTION).colorSpace   = libcamera::ColorSpace::Rec709;
                                           
   streamConfigs->sensorConfig                    = libcamera::SensorConfiguration();
	streamConfigs->sensorConfig->outputSize        = libcamera::Size(1920, 1080);
	streamConfigs->sensorConfig->bitDepth          = 12;
	
   log.info("validating configuration");
   CameraConfiguration::Status configStatus = streamConfigs->validate();
   switch (configStatus) {
      case CameraConfiguration::Status::Valid:     log.info("configuration is validated");
                                                   break;
      case CameraConfiguration::Status::Adjusted:  log.info("configuration adjusted");
                                                   break;
      case CameraConfiguration::Status::Invalid:   log.error("configuration invalid");
                                                   return false;
      default:                                     log.error("unknown config status received");
                                                   return false;
   }
   
   log.info("configuring camera stream");
   errorCode = camera->configure(streamConfigs.get());
   if (errorCode != 0) {
      log.error("failed to configure camera: error code", errorCode);
      return false;
   }
   
   for (int i = 0; i < (int)streamConfigs->size(); i++) {
      std::ostringstream message;
      message << "configured stream (index = " << i 
              << ", size = "        << streamConfigs->at(i).size.width << " x " << streamConfigs->at(i).size.height 
              << ", frameSize = "   << streamConfigs->at(i).frameSize
              << ", stride = "      << streamConfigs->at(i).stride 
              << ", pixelFormat = " << streamConfigs->at(i).pixelFormat.toString()
              << ", colorSpace = "  << streamConfigs->at(i).colorSpace.value().toString() << ")";
      log.info(message.str());
   }
   
   log.info("registering request completed callback");
   camera->requestCompleted.connect(this, &Camera::requestCompleted);
   
   if (!createRequests(streamConfigs.get())) {
      return false;
   }
     
   log.info("initializing capabilities");
   controlsToSet.reset(new ControlList());
   auto setControlsFunction = std::bind(&Camera::setControls, this, std::placeholders::_1);
   capabilities.reset(new CameraCapabilities(camera->controls(), setControlsFunction));

   return true;
}

libcamera::StreamConfiguration const & Camera::getStreamConfiguration(StreamType streamType) {
   return streamConfigs->at(static_cast<int>(streamType));
}

void Camera::enqueueRequest(libcamera::Request *request) {
   request->reuse(Request::ReuseFlag::ReuseBuffers);
   
   {
      std::lock_guard<std::mutex> guard(controlsToSetMutex);
      if (controlsToSet && !controlsToSet->empty()) {
         for (auto tuple : *controlsToSet) {
            log.info("telling camera module to set", 
                     libcamera::controls::controls.at(tuple.first)->name(), "to", tuple.second.toString());
         }
         request->controls() = std::move(*controlsToSet);
         controlsToSet.reset(new ControlList());
      }
   }
   
   std::lock_guard<std::mutex> guard(pendingRequestsMutex);
   int errorCode = camera->queueRequest(request);
   if (errorCode == 0) {
      pendingRequests++;
   } else {
      log.error("failed to queue request: error code", errorCode);
   }
}

void Camera::requestCompleted(Request *request) {
   {
      std::lock_guard<std::mutex> guard(pendingRequestsMutex);
      pendingRequests--;
   }
   
   if(request->status() != Request::Status::RequestComplete) {
      log.error("ignoring completed request because it is not completed");
   }
   
   if (!started) {
      return;
   }
   
   auto highResolutionStream      = streamConfigs->at(HIGH_RESOLUTION).stream();
   auto lowResolutionStream       = streamConfigs->at(LOW_RESOLUTION).stream();
   auto highResolutionFrameBuffer = request->findBuffer(highResolutionStream);
   auto lowResolutionFrameBuffer  = request->findBuffer(lowResolutionStream);
   auto ts                        = request->metadata().get(libcamera::controls::SensorTimestamp);
   int64_t timestamp_ns           = ts ? *ts : highResolutionFrameBuffer->metadata().timestamp;
	
   frameConsumer(highResolutionFrameBuffer, lowResolutionFrameBuffer, timestamp_ns / 1000);
   
   enqueueRequest(request);
}

bool Camera::start(FrameConsumer consumer) {    
   if (!initialized) {
      log.error("cannot start because not initialized");
      return false;
   }
   
   frameConsumer = consumer;
   
   log.info("starting capturing from camera");
   int errorCode = camera->start();
   if (errorCode != 0) {
      log.error("failed to start capturing from camera: error code", errorCode);
      return false;
   }
   
   log.info("enqueuing requests");
   for (unsigned int index = 0; index < requests.size(); index++) {
      enqueueRequest(requests[index].get());     
   }
   
   started = true;
   return true;
}

void Camera::stop() {
   if (!initialized || !started) {
      return;
   }
   
   log.info("stop called -> waiting for completion of pending requests");
   started = false;
   while(pendingRequests > 0) {
      std::this_thread::sleep_for(100ms);
   }
   log.info("stopping camera");
   int errorCode = camera->stop();
   if (errorCode != 0) {
      log.error("failed to stop camera: error code", errorCode);
   }
}

bool Camera::setControl(std::string controlName, float value) {
   if (capabilities) {
      return capabilities->setValue(controlName, value);
   } else {
      log.error("ignoring request to set control because no capabilities available");
      return false;
   }
}

void Camera::setControls(std::unique_ptr<ControlList> controls) {
   std::lock_guard<std::mutex> guard(controlsToSetMutex);
   for (auto tuple : *controls) {
      controlsToSet->set(tuple.first, tuple.second);
   }
}

bool Camera::isStarted() { 
   return started;
}

void Camera::setCapabilitiesListener(capabilities::CameraCapabilities::Listener& listener) {
   if (initialized) {
      capabilities->setListener(listener);
   }
}

Camera::~Camera() {  
   if (started) {
      log.info("stopping camera");
      camera->stop();
   }
   if (camera) {
      log.info("releasing camera");
      camera->release();
      camera.reset(); // this avoids problems when stopping the camera manager
   }
   if (cameraManager) {
      log.info("stopping camera manager");
      cameraManager->stop();
   }
   
   requests.clear();
   frameBuffers.clear();
}
