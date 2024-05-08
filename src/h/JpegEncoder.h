#ifndef JPEG_ENCODER_H
#define JPEG_ENCODER_H

#include <cstdint>
#include <stdio.h>
// stdio.h needs to get included before jpeglib.h 
// (see https://raw.githubusercontent.com/libjpeg-turbo/libjpeg-turbo/main/libjpeg.txt)
#include <jpeglib.h>

#include "libcamera/stream.h"

#include "Logging.h"

#if JPEG_LIB_VERSION_MAJOR > 9 || (JPEG_LIB_VERSION_MAJOR == 9 && JPEG_LIB_VERSION_MINOR >= 4)
typedef size_t jpeg_mem_len_t;
#else
typedef unsigned long jpeg_mem_len_t;
#endif

class JpegEncoder {
   public:
      struct JpegImage {
         void*        data;
         unsigned int size;
      };
      
      /**
       * quality        range [0,100]
       */
      JpegEncoder(libcamera::StreamConfiguration const &streamConfig, int quality);
      
      ~JpegEncoder();
      
      /**
       * Uses the provided YUV420 data of a frame to create a JPEG.
       * The returned struct contains a pointer to the data and the
       * its length. 
       *
       * ATTENTION: The receiver is responsible to free the returned memory!       
       */
      JpegImage encode(void* yuv420Data, unsigned int length);
      
   private:
      logging::Logger             log;
      struct jpeg_error_mgr       jerr;
      struct jpeg_compress_struct cinfo;
      unsigned int                inputHeight;
      unsigned int                inputStride;
      bool                        firstFrame;
};

#endif
