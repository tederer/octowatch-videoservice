#ifndef CPU_JPEG_ENCODER_H
#define CPU_JPEG_ENCODER_H

#include <cstdint>
#include <stdio.h>
// stdio.h needs to get included before jpeglib.h 
// (see https://raw.githubusercontent.com/libjpeg-turbo/libjpeg-turbo/main/libjpeg.txt)
#include <jpeglib.h>

#include "libcamera/stream.h"

#include "JpegEncoder.h"
#include "Logging.h"

#if JPEG_LIB_VERSION_MAJOR > 9 || (JPEG_LIB_VERSION_MAJOR == 9 && JPEG_LIB_VERSION_MINOR >= 4)
typedef size_t jpeg_mem_len_t;
#else
typedef unsigned long jpeg_mem_len_t;
#endif

class CpuJpegEncoder : public JpegEncoder {
   public:
      struct JpegImage {
         void*        data;
         unsigned int size;
      };
      
      /**
       * quality        range [0,100]
       */
      CpuJpegEncoder(libcamera::StreamConfiguration const &streamConfig, int quality);
      
      ~CpuJpegEncoder();
      
      
      /**
       * The callback gets called as soon as a JPEG is ready for sending.
       */
      void setOutputReadyCallback(JpegOutputReadyCallback callback) override;

      /**
       * Provides a new frame to the encoder for encoding.
       */
      void encode(libcamera::FrameBuffer *frameBuffer, int64_t timestamp_us) override;
      
   private:
      logging::Logger             log;
      struct jpeg_error_mgr       jerr;
      struct jpeg_compress_struct cinfo;
      unsigned int                inputHeight;
      unsigned int                inputStride;
      bool                        firstFrame;
      JpegOutputReadyCallback     outputReadyCallback;
};

#endif
