#ifndef JPEGENCODER_H
#define JPEGENCODER_H

#include <functional>

#include "libcamera/framebuffer.h"

/** 
 * ATTENTION: The data pointer is only valid as long as you are in the callback!
 */
typedef std::function<void(void    *data,       // pointer to the data of the NAL
                           size_t  bytesCount,  // size of the NAL in bytes
                           int64_t timestamp_us)>  JpegOutputReadyCallback;

class JpegEncoder {
   public:
      /**
       * The callback gets called as soon as a NAL is ready for sending.
       */
      virtual void setOutputReadyCallback(JpegOutputReadyCallback callback) = 0;

      /**
       * Provides a new frame to the encoder for encoding.
       */
      virtual void encode(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us) = 0;
};

#endif
