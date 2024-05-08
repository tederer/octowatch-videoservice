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

#define NUM_OUTPUT_BUFFERS    6
#define NUM_CAPTURE_BUFFERS   12

typedef std::function<void(void    *data,       // point to the data of the NAL
                           size_t  bytesCount,  // size of the NAL in bytes
                           int64_t timestamp,   
                           bool    keyframe)>  OutputReadyCallback;

class H264Encoder {
   public:
      H264Encoder(libcamera::StreamConfiguration const &streamConfig);
      
      ~H264Encoder();

      /**
       * Gets called as soon as a NAL is ready for sending.
       *
       * ATTENTION: The data pointer is only valid as long as your
       *            in the callback!
       */
      void setOutputReadyCallback(OutputReadyCallback callback);

      /**
       * Provides a new frame to the encoder for encoding.
       */
      void encode(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us);

   private:
      struct BufferDescription {
         void   *mem;
         size_t size;
      };
      
      struct OutputItem {
         void         *mem;
         size_t       bytes_used;
         size_t       length;
         unsigned int index;
         bool         keyframe;
         int64_t      timestamp_us;
      };
      
      bool applyDeviceParam(unsigned long ctl, void *arg);
      int get_v4l2_colorspace(std::optional<libcamera::ColorSpace> const &cs);
      
      void pollThread();
      void outputThread();

      logging::Logger         log;
      OutputReadyCallback     outputReadyCallback;
      bool                    abortPoll_;
      bool                    abortOutput_;
      int                     encoderFileDescriptor;
      BufferDescription       buffers_[NUM_CAPTURE_BUFFERS];
      int                     num_capture_buffers_;
      std::thread             poll_thread_;
      std::mutex              inputBufferAvailableMutex;
      std::queue<int>         availableInputBuffers;
      std::queue<OutputItem>  output_queue_;
      std::mutex              output_mutex_;
      std::condition_variable output_cond_var_;
      std::thread             output_thread_;
};

#endif
