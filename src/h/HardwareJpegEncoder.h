#ifndef HARDWAREJPEGENCODER_H
#define HARDWAREJPEGENCODER_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "libcamera/color_space.h"
#include "libcamera/stream.h"

#include "JpegEncoder.h"
#include "Logging.h"

#define HARDWARE_JPEG_ENCODER_BUFFER_COUNT    1

class HardwareJpegEncoder : public JpegEncoder {
   public:
      /**
       * quality        range [1,100]
       */
      HardwareJpegEncoder(libcamera::StreamConfiguration const &streamConfig, int quality);
      
      ~HardwareJpegEncoder();

      /**
       * The callback gets called as soon as a JPEG is ready for sending.
       */
      void setOutputReadyCallback(JpegOutputReadyCallback callback) override;

      /**
       * Provides a new frame to the encoder for encoding.
       */
      void encode(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us) override;

   private:
      struct JpegImage {
         void         *data;
         size_t       bytesUsed;
         size_t       length;
         unsigned int index;
         int64_t      timestamp_us;
      };
      
      void v4l2Cmd(unsigned long ctl, void *arg, std::string errorMessage);
      int getV4l2Colorspace(std::optional<libcamera::ColorSpace> const &libcameraColorSpace);
      
      void openEncoderDevice();
      void setJpegQuality(int quality);
      void configureInputFormat(libcamera::StreamConfiguration const &streamConfig);
      void configureOutputFormat();
      void createInputBuffers();
      void createOutputBuffers();
      void startInputStream();
      void startOutputStream();
      
      void pollThreadTask();
      void outputThreadTask();

      logging::Logger         log;
      bool                    quitPollThread;
      bool                    quitOutputThread;
      bool                    v4l2CommandError;
      JpegOutputReadyCallback outputReadyCallback;
      int                     encoderFileDescriptor;
      void*                   outputBufferData[HARDWARE_JPEG_ENCODER_BUFFER_COUNT];
      std::queue<int>         availableInputBuffers;
      std::queue<int>         inputBuffersReadyToReuse;
      std::queue<JpegImage>   jpegsReadyToConsume;
      std::mutex              availableInputBuffersMutex;
      std::mutex              jpegsReadyMutex;
      std::mutex              inputBuffersReadyToReuseMutex;
      std::condition_variable jpegsReadyCondition;
      std::thread             pollThread;
      std::thread             outputThread;
};

#endif
