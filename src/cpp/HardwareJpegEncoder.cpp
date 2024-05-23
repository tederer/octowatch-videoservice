#include <chrono>
#include <iostream>
#include <sstream>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include "HardwareJpegEncoder.h"


using libcamera::ColorSpace;
using libcamera::FrameBuffer;
using libcamera::StreamConfiguration;

using logging::Logger;

using namespace std::chrono_literals;

void HardwareJpegEncoder::setOutputReadyCallback(JpegOutputReadyCallback callback) {
   outputReadyCallback = callback; 
}

void HardwareJpegEncoder::v4l2Cmd(unsigned long ctl, void *arg, std::string errorMessage) {
   if (v4l2CommandError) {
      return;
   }
   
	if (ioctl(encoderFileDescriptor, ctl, arg) == -1) {
      v4l2CommandError = true;
      std::ostringstream logMessage;
      logMessage << "ERROR: " << errorMessage << ": ";
      switch(errno) {
         case EBADF: logMessage << "file descriptor is not a valid file descriptor (EBADF)";
                     break;
         case EFAULT: logMessage << "argp references an inaccessible memory area (EFAULT)";
                     break;
         case EINVAL: logMessage << "request or argp is not valid (EINVAL)";
                     break;
         case ENOTTY: logMessage << "file descriptor is not associated with a character special device (ENOTTY)";
                     break;
      }
      throw std::runtime_error(logMessage.str());
   }
}

int HardwareJpegEncoder::getV4l2Colorspace(std::optional<ColorSpace> const &libcameraColorSpace) {
	if (libcameraColorSpace == ColorSpace::Rec709) {
		return V4L2_COLORSPACE_REC709;
   } else if (libcameraColorSpace == ColorSpace::Smpte170m) {
		return V4L2_COLORSPACE_SMPTE170M;
   }
   
	return V4L2_COLORSPACE_SMPTE170M;
}


void HardwareJpegEncoder::openEncoderDevice() {
   std::string deviceName = "/dev/video31";
	encoderFileDescriptor = open(deviceName.c_str(), O_RDWR, 0);
	if (encoderFileDescriptor < 0) {
		throw std::runtime_error("failed to open V4L2 JPEG encoder");
   }
	log.info("opened HardwareJpegEncoder on", deviceName);
}
  
void HardwareJpegEncoder::setJpegQuality(int quality) {
   v4l2_control control = {};
   control.id           = V4L2_CID_JPEG_COMPRESSION_QUALITY;
   control.value        = quality;
   
	v4l2Cmd(VIDIOC_S_CTRL, &control, "failed to set quality");
   log.info("quality =", quality);
}
   
void HardwareJpegEncoder::configureInputFormat(StreamConfiguration const &streamConfig) {
   struct v4l2_format inFormat       = {};
   int                v4l2Colorspace = getV4l2Colorspace(streamConfig.colorSpace);
   
   log.info("streamConfig( width =", streamConfig.size.width, 
            ", height =", streamConfig.size.height, ", stride =", streamConfig.stride, ",v4l2Colorspace =", v4l2Colorspace, ")");
   

   inFormat.type                                 = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	inFormat.fmt.pix_mp.width                     = streamConfig.size.width;
	inFormat.fmt.pix_mp.height                    = streamConfig.size.height;
	inFormat.fmt.pix_mp.pixelformat               = V4L2_PIX_FMT_YUV420;
	inFormat.fmt.pix_mp.plane_fmt[0].bytesperline = streamConfig.stride;
	inFormat.fmt.pix_mp.field                     = V4L2_FIELD_NONE;
   inFormat.fmt.pix_mp.colorspace                = v4l2Colorspace;
   inFormat.fmt.pix_mp.num_planes                = 1;

   v4l2Cmd(VIDIOC_S_FMT, &inFormat, "failed to set input format");
   
   bool returnedFormatMatches = inFormat.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_YUV420;
   std::ostringstream inPixelFormat;
   inPixelFormat << "\t* pixelformat: 0x" << std::hex << inFormat.fmt.pix_mp.pixelformat 
      << std::dec << " (" << (returnedFormatMatches ? "V4L2_PIX_FMT_YUV420" : "format changed!") << ")";
   log.info("input format:");
   log.info("\t* dimension:  ", inFormat.fmt.pix_mp.width, "x", inFormat.fmt.pix_mp.height);
   log.info("\t* planes:     ", (int)inFormat.fmt.pix_mp.num_planes);
   log.info("\t* stride:     ", inFormat.fmt.pix_mp.plane_fmt[0].bytesperline);
   log.info("\t* size:       ", inFormat.fmt.pix_mp.plane_fmt[0].sizeimage);
   log.info(inPixelFormat.str());
}

void HardwareJpegEncoder::configureOutputFormat() {
   struct v4l2_format outFormat = {};
	outFormat.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	outFormat.fmt.pix.width       = 800;
	outFormat.fmt.pix.height      = 608;
	outFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
	outFormat.fmt.pix.field       = V4L2_FIELD_NONE;
   
   v4l2Cmd(VIDIOC_S_FMT, &outFormat, "failed to set output format");
   
   bool returnedFormatMatches = outFormat.fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG;
   std::ostringstream outPixelFormat;
   outPixelFormat << "\t* pixelformat: 0x" << std::hex << outFormat.fmt.pix.pixelformat << std::dec << " (" << (returnedFormatMatches ? "V4L2_PIX_FMT_JPEG" : "format changed!") << ")";
   log.info("output format:");
   log.info("\t* dimension:  ", outFormat.fmt.pix.width,"x", outFormat.fmt.pix.height);
   log.info(outPixelFormat.str());
}

void HardwareJpegEncoder::createInputBuffers() {
   struct v4l2_requestbuffers inputBufferRequest = {};
	inputBufferRequest.count                      = HARDWARE_JPEG_ENCODER_BUFFER_COUNT;
	inputBufferRequest.type                       = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	inputBufferRequest.memory                     = V4L2_MEMORY_DMABUF;

	v4l2Cmd(VIDIOC_REQBUFS, &inputBufferRequest, "failed to request input buffer(s)");
   
   log.info("input buffer(s):");
   log.info("\t* count     =", inputBufferRequest.count);
      
   for (unsigned int index = 0; index < inputBufferRequest.count; index++) {
		std::lock_guard<std::mutex> lock(availableInputBuffersMutex);
		availableInputBuffers.push(index);
   }
   
   for (unsigned int index = 0; index < inputBufferRequest.count; index++) {
      struct v4l2_plane  inputPlanes[1];
      struct v4l2_buffer inputBufferQuery = {};
      inputBufferQuery.type               = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      inputBufferQuery.memory             = V4L2_MEMORY_DMABUF;
      inputBufferQuery.index              = index;
      inputBufferQuery.length             = 1;
      inputBufferQuery.m.planes           = inputPlanes;
      
      {
         std::ostringstream message;
         message << "failed to query input buffer " << index;
         v4l2Cmd(VIDIOC_QUERYBUF, &inputBufferQuery, message.str());
      }
      
      unsigned int inputPlaneCount = inputBufferQuery.length;
      
      if (inputPlaneCount != 1) {
         std::ostringstream message;
         message << "input buffer plane count is " << inputPlaneCount << " instead of 1.";
         throw std::runtime_error(message.str());
      }
      
      auto inputBufferSize   = inputBufferQuery.m.planes[0].length;
      auto inputBufferOffset = inputBufferQuery.m.planes[0].m.mem_offset;
      
      log.info("\t* bytesused =", inputBufferQuery.m.planes[0].bytesused);
      log.info("\t* length    =", inputBufferSize);   
      log.info("\t* offset    =", inputBufferOffset);   
   }
}   

void HardwareJpegEncoder::createOutputBuffers() {
   struct v4l2_requestbuffers outputBufferRequest = {};
	outputBufferRequest.count                      = HARDWARE_JPEG_ENCODER_BUFFER_COUNT;
	outputBufferRequest.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	outputBufferRequest.memory                     = V4L2_MEMORY_MMAP;

	v4l2Cmd(VIDIOC_REQBUFS, &outputBufferRequest, "failed to request output buffer(s)");
   
   log.info("output buffer(s):");
   log.info("\t* count     =", outputBufferRequest.count);
      
   for (unsigned int index = 0; index < outputBufferRequest.count; index++) {
      struct v4l2_plane  outputPlanes[1];
      struct v4l2_buffer outputBufferQuery = {};
      outputBufferQuery.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      outputBufferQuery.memory             = V4L2_MEMORY_MMAP;
      outputBufferQuery.index              = index;
      outputBufferQuery.length             = 1;
      outputBufferQuery.m.planes           = outputPlanes;
      
      {
         std::ostringstream message;
         message << "failed to query output buffer " << index;
         v4l2Cmd(VIDIOC_QUERYBUF, &outputBufferQuery, message.str());
      }
      
      unsigned int outputPlaneCount = outputBufferQuery.length;
      
      if (outputPlaneCount != 1) {
         std::ostringstream message;
         message << "output buffer plane count is " << outputPlaneCount << " instead of 1.";
         throw std::runtime_error(message.str());
      }
      
      auto outputBufferSize   = outputBufferQuery.m.planes[0].length;
      auto outputBufferOffset = outputBufferQuery.m.planes[0].m.mem_offset;
      
      log.info("\t* bytesused =", outputBufferQuery.m.planes[0].bytesused);
      log.info("\t* length    =", outputBufferSize);   
      log.info("\t* offset    =", outputBufferOffset);   
   
      unsigned char* outputBuffer = (u_int8_t*) mmap(NULL, outputBufferSize, 
                                       PROT_READ | PROT_WRITE, MAP_SHARED, encoderFileDescriptor, outputBufferOffset);
      if (outputBuffer == MAP_FAILED) {
         std::ostringstream message;
         message << "failed to map output buffer: errno " << errno;
         throw std::runtime_error(message.str());
      }
      
      outputBufferData[index] = outputBuffer;
         
      v4l2Cmd(VIDIOC_QBUF, &outputBufferQuery, "failed to queue output buffer(s)");
   }
}

void HardwareJpegEncoder::startInputStream() {
   unsigned int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	v4l2Cmd(VIDIOC_STREAMON, &type, "failed to start input");
}

void HardwareJpegEncoder::startOutputStream() {
   unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	v4l2Cmd(VIDIOC_STREAMON, &type, "failed to start output");
}
   
HardwareJpegEncoder::HardwareJpegEncoder(StreamConfiguration const &streamConfig, int quality)
	: log("HardwareJpegEncoder"), quitPollThread(false), quitOutputThread(false), v4l2CommandError(false) {
   
   if ((quality < 1) || (quality > 100)) {
      std::ostringstream message;
      message << "image quality " << quality << " is not within [1,100].";
      throw std::runtime_error(message.str());
   }
   
   openEncoderDevice();
   setJpegQuality(quality);
   configureInputFormat(streamConfig);
   configureOutputFormat();
   createInputBuffers();
   createOutputBuffers();
   startInputStream();
   startOutputStream();

   log.info("encoder started");

	outputThread = std::thread(&HardwareJpegEncoder::outputThreadTask, this);
	pollThread   = std::thread(&HardwareJpegEncoder::pollThreadTask,   this);
}

HardwareJpegEncoder::~HardwareJpegEncoder() {
	quitPollThread   = true;
	quitOutputThread = true;
	pollThread.join();
   log.info("poll thread finished");
	outputThread.join();
   log.info("output thread finished");

	unsigned int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	v4l2Cmd(VIDIOC_STREAMOFF, &type, "failed to stop input");
   
   type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	v4l2Cmd(VIDIOC_STREAMOFF, &type, "failed to stop output");
   
	close(encoderFileDescriptor);
	log.info("HardwareJpegEncoder closed");
}

void HardwareJpegEncoder::encode(FrameBuffer *frameBuffer, int64_t timestamp_us) {
   
   if (logging::minLevel == DEBUG) {
      log.debug("new frame to encode ( timestamp =", timestamp_us, ")");
   }
   
   int indexOfFreeBuffer;
	{
		std::lock_guard<std::mutex> lock(availableInputBuffersMutex);
		if (availableInputBuffers.empty()) {
			log.warning("no input buffer available -> ignoring frame");
         return;
      }
		indexOfFreeBuffer = availableInputBuffers.front();
		availableInputBuffers.pop();
	}
   
   auto firstPlane = frameBuffer->planes()[0];
   
   v4l2_buffer buf       = {};
	v4l2_plane  planes[1] = {};
   
   buf.index                   = indexOfFreeBuffer;                 
	buf.type                    = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; 
	buf.field                   = V4L2_FIELD_NONE;                   
	buf.memory                  = V4L2_MEMORY_DMABUF;                  
	buf.length                  = 1;                        
	buf.timestamp.tv_sec        = timestamp_us / 1000000;
	buf.timestamp.tv_usec       = timestamp_us % 1000000;
	buf.m.planes                = planes;
	buf.m.planes[0].m.fd        = firstPlane.fd.get();               
	buf.m.planes[0].bytesused   = firstPlane.length;                       
	buf.m.planes[0].length      = firstPlane.length + firstPlane.offset;                         
   buf.m.planes[0].data_offset = firstPlane.offset;
   
   v4l2Cmd(VIDIOC_QBUF, &buf, "failed to enqueue input buffer");
}

/**
 * This task waits for the completion of encoding an JPEG. As soon as an
 * image is available it moves one input buffer back to the queue of 
 * available buffers and dequeues the output buffer (containing the JPEG).
 * The JPEG gets put into another queue processed by the outputThreadTask.
 * This decouples the consumption of the JPEG from moving around buffers.
 */
void HardwareJpegEncoder::pollThreadTask() {
	while (true) {
      pollfd driverEvent = { encoderFileDescriptor, POLLIN, 0 };
		// Wait for data to read (POLLIN) from the encoder device and timeout
      // after 200 ms.
      //
      // returnValue >  0 number of elements in the pollfds whose revents 
      //                  fields have been set to POLLIN
      //             =  0 time out
      //             = -1 error
		int returnValue = poll(&driverEvent, 1, 200);
		{
			std::lock_guard<std::mutex> lock(availableInputBuffersMutex);
			if (quitPollThread && availableInputBuffers.size() == HARDWARE_JPEG_ENCODER_BUFFER_COUNT) {
				break;
         }
		}
      
		if (returnValue == -1) {
			if (errno == EINTR) {
				continue;
         }
			throw std::runtime_error("unexpected errno " + std::to_string(errno) + " from poll");
		}
      
		if (driverEvent.revents & POLLIN) { // JPEG image ready to provide to consumer/callback
         v4l2_buffer buf       = {};
			v4l2_plane  planes[1] = {};
         
			buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
			buf.memory   = V4L2_MEMORY_DMABUF;
			buf.length   = 1;
			buf.m.planes = planes;
         
         v4l2Cmd(VIDIOC_DQBUF, &buf, "failed to dequeue input buffer");
         {
            std::lock_guard<std::mutex> lock(inputBuffersReadyToReuseMutex);
            inputBuffersReadyToReuse.push(buf.index);
         }

			buf = {};
			memset(planes, 0, sizeof(planes));
         
			buf.type       = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			buf.memory     = V4L2_MEMORY_MMAP;
			buf.length     = 1;
			buf.m.planes   = planes;
         
			v4l2Cmd(VIDIOC_DQBUF, &buf, "failed to dequeue output buffer");
         int64_t timestamp_us = (buf.timestamp.tv_sec * (int64_t)1000000) + buf.timestamp.tv_usec;
         JpegImage jpegImage = { outputBufferData[buf.index],
                                 buf.m.planes[0].bytesused,
                                 buf.m.planes[0].length,
									      buf.index,
									      timestamp_us };
         {
            std::lock_guard<std::mutex> lock(jpegsReadyMutex);
            jpegsReadyToConsume.push(jpegImage);
         }
         jpegsReadyCondition.notify_one();
		}
	}
}

/**
 * This task waits for JPEGs enqueued by pollThreadTask and provides
 * them to the consumer/callback. After consumption the output buffer
 * gets enqueue again.
 */
void HardwareJpegEncoder::outputThreadTask() {
	JpegImage jpegImage;
	while (true) {
		{
			std::unique_lock<std::mutex> lock(jpegsReadyMutex);
			while (true) {
				if (quitOutputThread && jpegsReadyToConsume.empty()) {
					return;
            }
				if (!jpegsReadyToConsume.empty()) {
					jpegImage = jpegsReadyToConsume.front();
					jpegsReadyToConsume.pop();
					break;
				} else {
					jpegsReadyCondition.wait_for(lock, 200ms);
            }
			}
		}

      if (outputReadyCallback) {
         outputReadyCallback(jpegImage.data, jpegImage.bytesUsed, jpegImage.timestamp_us);
		}
      
      v4l2_buffer buf      = {};
		v4l2_plane planes[1] = {};
      
		buf.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory                 = V4L2_MEMORY_MMAP;
		buf.index                  = jpegImage.index;
		buf.length                 = 1;
		buf.m.planes               = planes;
		buf.m.planes[0].bytesused  = 0;
		buf.m.planes[0].length     = jpegImage.length;
      
		v4l2Cmd(VIDIOC_QBUF, &buf, "failed to enqueue output buffer");
      
      int inputBufferIndex = -1;
      std::lock_guard<std::mutex> lock(inputBuffersReadyToReuseMutex);
      {
         if (!inputBuffersReadyToReuse.empty()) {
            inputBufferIndex = inputBuffersReadyToReuse.front();
            inputBuffersReadyToReuse.pop();
         }    
      }
      
      if (inputBufferIndex >= 0) {
         std::lock_guard<std::mutex> lock(availableInputBuffersMutex);
         availableInputBuffers.push(inputBufferIndex);   
      }
	}
}
