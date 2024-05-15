#ifndef H264ENCODER_H
#define H264ENCODER_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "libcamera/color_space.h"
#include "libcamera/stream.h"

#include "Logging.h"

#define H264_INPUT_BUFFER_COUNT    6
#define H264_OUTPUT_BUFFER_COUNT   12

/**
 * ATTENTION: The data pointer is only valid as long as you are
 *            in the callback!
 */
typedef std::function<void(void    *data,       // pointer to the data of the NAL
                           size_t  bytesCount,  // size of the NAL in bytes
                           int64_t timestamp,   
                           bool    keyframe)>  OutputReadyCallback;

class H264Encoder {
   public:
      H264Encoder(libcamera::StreamConfiguration const &streamConfig);
      
      ~H264Encoder();

      /**
       * The callback gets called as soon as a NAL is ready for sending.
       */
      void setOutputReadyCallback(OutputReadyCallback callback);

      /**
       * Provides a new frame to the encoder for encoding.
       */
      void encode(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us);

   private:
      struct H264Nal {
         void         *mem;
         size_t       bytes_used;
         size_t       length;
         unsigned int index;
         bool         keyframe;
         int64_t      timestamp_us;
      };
      
      void v4l2Cmd(unsigned long ctl, void *arg, std::string errorMessage);
      int get_v4l2_colorspace(std::optional<libcamera::ColorSpace> const &libcameraColorSpace);
      
      void pollThreadTask();
      void outputThreadTask();

      logging::Logger         log;
      OutputReadyCallback     outputReadyCallback;
      bool                    quitPollThread;
      bool                    quitOutputThread;
      bool                    v4l2CommandError;
      int                     encoderFileDescriptor;
      void*                   outputBufferData[H264_OUTPUT_BUFFER_COUNT];
      std::mutex              inputBufferAvailableMutex;
      std::queue<int>         availableInputBuffers;
      std::queue<H264Nal>     nalsReadyToConsume;
      std::mutex              nalsReadyMutex;
      std::condition_variable nalsReadyCondition;
      std::thread             pollThread;
      std::thread             outputThread;
};

#endif
