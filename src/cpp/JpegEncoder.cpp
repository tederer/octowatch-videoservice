#include <vector>

#include "JpegEncoder.h"

using libcamera::StreamConfiguration;

JpegEncoder::JpegEncoder(StreamConfiguration const &streamConfig, int quality)
   : log("JpegEncoder"), 
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
}

JpegEncoder::~JpegEncoder() {
   	jpeg_destroy_compress(&cinfo);
}

JpegEncoder::JpegImage JpegEncoder::encode(void* yuv420Data, unsigned int length) {

	uint8_t*       jpegBuffer = nullptr;
   jpeg_mem_len_t jpegLength = 0;
   uint8_t*       input      = (uint8_t*) yuv420Data;
   
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

	for (uint8_t *Y_row = Y, *U_row = U, *V_row = V; cinfo.next_scanline < inputHeight;)
	{
		for (int i = 0; i < 16; i++, Y_row += inputStride)
			y_rows[i] = std::min(Y_row, Y_max);
		for (int i = 0; i < 8; i++, U_row += halfStride, V_row += halfStride)
			u_rows[i] = std::min(U_row, U_max), v_rows[i] = std::min(V_row, V_max);

		JSAMPARRAY rows[] = { y_rows, u_rows, v_rows };
		jpeg_write_raw_data(&cinfo, rows, 16);
	}

   jpeg_finish_compress(&cinfo);

   return {jpegBuffer, jpegLength};
}