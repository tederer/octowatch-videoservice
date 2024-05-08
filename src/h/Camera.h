#ifndef CAMERA_H
#define CAMERA_H

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "libcamera/camera.h"
#include "libcamera/camera_manager.h"
#include "libcamera/controls.h"
#include "libcamera/framebuffer.h"
#include "libcamera/request.h"

#include "CameraCapabilities.h"
#include "DmaHeap.h"
#include "Logging.h"

typedef std::function<void(libcamera::FrameBuffer *highResolutionFrameBuffer, 
                           libcamera::FrameBuffer *lowResolutionFrameBuffer,
                           int64_t timestamp)> FrameConsumer;

enum StreamType { HIGH_RESOLUTION = 0, LOW_RESOLUTION = 1 };

/**
 * A camera instance produces the following two streams from the same 
 * camera module.
 *
 *  * high resolution: 1920 x 1080, YUV420
 *  * low  resolution:  800 x  600, YUV420
 */
class Camera {
   public:
      Camera();
      
      ~Camera();
      
      /**
       * Starts the camera and returns true if success, otherwise false.
       * The frameConsumer needs to be fast enough to finish between two 
       * frames, otherwise the frame rate degrades.
       */      
      bool start(FrameConsumer frameConsumer);
      
      void stop();
      
      bool isStarted();
      
      void setCapabilitiesListener(capabilities::CameraCapabilities::Listener& capabilitiesListener);
      
      /**
       * Method for the camera control to change the value of a control.
       */
      bool setControl(std::string control, float value);
      
      /**
       * Returns the config of the stream identified by HIGH_RESOLUTION or LOW_RESOLUTION.
       */
      libcamera::StreamConfiguration const & getStreamConfiguration(StreamType streamType);
      
   private:
      bool initialize();
      
      void requestCompleted(libcamera::Request *request);
   
      bool createRequests(libcamera::CameraConfiguration *streamConfigs);
      
      void enqueueRequest(libcamera::Request *request);
      
      /**
       * Gets called by the capabilities to change the value(s) of control(s).
       */
      void setControls(std::unique_ptr<libcamera::ControlList> controls);
      
      logging::Logger                                      log;
      std::unique_ptr<libcamera::CameraConfiguration>      streamConfigs;
      std::shared_ptr<libcamera::Camera>                   camera;
      std::unique_ptr<libcamera::CameraManager>            cameraManager;
      std::unique_ptr<capabilities::CameraCapabilities>    capabilities;
      std::unique_ptr<libcamera::ControlList>              controlsToSet;
      std::vector<std::unique_ptr<libcamera::FrameBuffer>> frameBuffers;
      std::vector<std::unique_ptr<libcamera::Request>>     requests;
      FrameConsumer                                        frameConsumer;
      std::mutex                                           pendingRequestsMutex;
      std::mutex                                           controlsToSetMutex;
      DmaHeap                                              dmaHeap;
      bool                                                 started;
      bool                                                 initialized;
      int                                                  pendingRequests;
};

#endif
