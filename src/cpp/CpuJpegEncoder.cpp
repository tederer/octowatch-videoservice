#include <vector>
#include <sys/mman.h>

#include "CpuJpegEncoder.h"

using libcamera::FrameBuffer;
using libcamera::StreamConfiguration;

CpuJpegEncoder::CpuJpegEncoder(StreamConfiguration const &streamConfig, int quality)
   : log("CpuJpegEncoder"), 
     inputHeight(streamConfig.size.height), 
     inputStride(streamConfig.stride), 
     firstFrame(true) {
        
   cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
   
   // properties of input image
   cinfo.image_width       = streamConfig.size.width;
	cinfo.image_height      = streamConfig.size.height;
	cinfo.input_components  = 3;
	cinfo.in_color_space    = JCS_YCbCr;

	jpeg_set_defaults(&cinfo);
	cinfo.raw_data_in = true;
	jpeg_set_quality(&cinfo, quality, true);
   log.info("quality =", quality);
}

CpuJpegEncoder::~CpuJpegEncoder() {
   	jpeg_destroy_compress(&cinfo);
}

void CpuJpegEncoder::setOutputReadyCallback(JpegOutputReadyCallback callback) {
   outputReadyCallback = callback; 
}

void CpuJpegEncoder::encode(FrameBuffer *frameBuffer, int64_t timestamp_us) {
   auto firstPlane          = frameBuffer->planes()[0];
   void* frameBufferContent = mmap(nullptr, firstPlane.length, PROT_READ, MAP_SHARED,
                                   firstPlane.fd.get(), firstPlane.offset);
   
   if (frameBufferContent == MAP_FAILED) {
      log.error("failed to map DMA buffer, errno", errno, "-> ignoring frame");
      return;
   }
   
   auto start = std::chrono::steady_clock::now();
   
	uint8_t*       jpegBuffer = nullptr;
   jpeg_mem_len_t jpegLength = 0;
   uint8_t*       input      = (uint8_t*)frameBufferContent + firstPlane.offset;
   
   jpeg_mem_dest(&cinfo, &jpegBuffer, &jpegLength);
   jpeg_start_compress(&cinfo, true);
      
	int halfStride = inputStride / 2;
   int uvSize     = halfStride * (inputHeight / 2);
	uint8_t *Y     = input;
	uint8_t *U     = Y + inputStride * inputHeight;
	uint8_t *V     = U + uvSize;
	uint8_t *Y_max = U - inputStride;
	uint8_t *U_max = V - halfStride;
	uint8_t *V_max = U_max + uvSize;

	JSAMPROW y_rows[16];
	JSAMPROW u_rows[8];
	JSAMPROW v_rows[8];

	for (uint8_t *Y_row = Y, *U_row = U, *V_row = V; cinfo.next_scanline < inputHeight;) {
		for (int i = 0; i < 16; i++, Y_row += inputStride) {
			y_rows[i] = std::min(Y_row, Y_max);
      }
		for (int i = 0; i < 8; i++, U_row += halfStride, V_row += halfStride) {
			u_rows[i] = std::min(U_row, U_max), v_rows[i] = std::min(V_row, V_max);
      }
		JSAMPARRAY rows[] = { y_rows, u_rows, v_rows };
		jpeg_write_raw_data(&cinfo, rows, 16);
	}

   jpeg_finish_compress(&cinfo);

   if (munmap(frameBufferContent, firstPlane.length) != 0) {
      log.error("failed to unmap DMA buffer");
   } 
   
   auto end = std::chrono::steady_clock::now();
   std::chrono::duration<double> diff = end - start;
   log.debug("JPEG encoding duration =", (int)(diff.count() * 1000), "ms"); 
      
   outputReadyCallback(jpegBuffer, jpegLength, timestamp_us);
}