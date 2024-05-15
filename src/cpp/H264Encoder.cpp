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

#include "H264Encoder.h"


using libcamera::ColorSpace;
using libcamera::FrameBuffer;
using libcamera::StreamConfiguration;

using logging::Logger;

using namespace std::chrono_literals;

void H264Encoder::v4l2Cmd(unsigned long ctl, void *arg, std::string errorMessage) {
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

int H264Encoder::get_v4l2_colorspace(std::optional<ColorSpace> const &libcameraColorSpace) {
	if (libcameraColorSpace == ColorSpace::Rec709) {
		return V4L2_COLORSPACE_REC709;
   } else if (libcameraColorSpace == ColorSpace::Smpte170m) {
		return V4L2_COLORSPACE_SMPTE170M;
   }
   
	return V4L2_COLORSPACE_SMPTE170M;
}

void H264Encoder::setOutputReadyCallback(OutputReadyCallback callback) {
   outputReadyCallback = callback; 
}

H264Encoder::H264Encoder(StreamConfiguration const &streamConfig)
	: log("H264Encoder"), quitPollThread(false), quitOutputThread(false), v4l2CommandError(false) {
      
   std::string deviceName = "/dev/video11";
	encoderFileDescriptor = open(deviceName.c_str(), O_RDWR, 0);
	if (encoderFileDescriptor < 0) {
		throw std::runtime_error("failed to open V4L2 H264 encoder");
   }
	log.info("Opened H264Encoder on", deviceName, "as fd", encoderFileDescriptor);

	v4l2_control ctrl = {};
	
   ctrl.id    = V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER;
   ctrl.value = 1;
   
   v4l2Cmd(VIDIOC_S_CTRL, &ctrl, "failed to set inline headers");
   
	v4l2_format fmt = {};
	fmt.type                                  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.width                      = streamConfig.size.width;
	fmt.fmt.pix_mp.height                     = streamConfig.size.height;
	fmt.fmt.pix_mp.pixelformat                = V4L2_PIX_FMT_YUV420;
	fmt.fmt.pix_mp.plane_fmt[0].bytesperline  = streamConfig.stride;
	fmt.fmt.pix_mp.field                      = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.colorspace                 = get_v4l2_colorspace(streamConfig.colorSpace);
	fmt.fmt.pix_mp.num_planes                 = 1;
	
   v4l2Cmd(VIDIOC_S_FMT, &fmt, "failed to set output format");
   
	fmt = {};
	fmt.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width                      = 1920;
	fmt.fmt.pix_mp.height                     = 1080;
	fmt.fmt.pix_mp.pixelformat                = V4L2_PIX_FMT_H264;
	fmt.fmt.pix_mp.field                      = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.colorspace                 = V4L2_COLORSPACE_DEFAULT;
	fmt.fmt.pix_mp.num_planes                 = 1;
	fmt.fmt.pix_mp.plane_fmt[0].bytesperline  = 0;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage     = 512 << 10;
   
	v4l2Cmd(VIDIOC_S_FMT, &fmt, "failed to set capture format");
   
	double framerate            = 30;
   struct v4l2_streamparm parm = {};
   parm.type                                 = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
   parm.parm.output.timeperframe.numerator   = 90000.0 / framerate;
   parm.parm.output.timeperframe.denominator = 90000;
   
   v4l2Cmd(VIDIOC_S_PARM, &parm, "failed to set streamparm");
   
	v4l2_requestbuffers reqbufs = {};
	reqbufs.count               = H264_INPUT_BUFFER_COUNT;
	reqbufs.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbufs.memory              = V4L2_MEMORY_DMABUF;
   
	v4l2Cmd(VIDIOC_REQBUFS, &reqbufs, "request for output buffers failed");
   
	log.info("Got", reqbufs.count, "output buffers");

	for (unsigned int i = 0; i < reqbufs.count; i++) {
		availableInputBuffers.push(i);
   }
   
	reqbufs        = {};
	reqbufs.count  = H264_OUTPUT_BUFFER_COUNT;
	reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbufs.memory = V4L2_MEMORY_MMAP;
	v4l2Cmd(VIDIOC_REQBUFS, &reqbufs, "request for capture buffers failed");
   
	log.info("Got", reqbufs.count, "capture buffers");
	
	for (unsigned int i = 0; i < reqbufs.count; i++) {
		v4l2_plane planes[VIDEO_MAX_PLANES];
		v4l2_buffer buffer = {};
		buffer.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buffer.memory      = V4L2_MEMORY_MMAP;
		buffer.index       = i;
		buffer.length      = 1;
		buffer.m.planes    = planes;
      
		v4l2Cmd(VIDIOC_QUERYBUF, &buffer, "failed to capture query buffer " + std::to_string(i));
      
		outputBufferData[i] = mmap(0, buffer.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, encoderFileDescriptor,
							   buffer.m.planes[0].m.mem_offset);
		if (outputBufferData[i] == MAP_FAILED) {
			throw std::runtime_error("failed to mmap capture buffer " + std::to_string(i));
      }
		
      v4l2Cmd(VIDIOC_QBUF, &buffer, "failed to queue capture buffer " + std::to_string(i));
	}

	v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	v4l2Cmd(VIDIOC_STREAMON, &type, "failed to start output streaming");
   
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	v4l2Cmd(VIDIOC_STREAMON, &type, "failed to start capture streaming");
   
	log.info("encoder started");

	outputThread = std::thread(&H264Encoder::outputThreadTask, this);
	pollThread   = std::thread(&H264Encoder::pollThreadTask,   this);
}

H264Encoder::~H264Encoder() {
	quitPollThread   = true;
	quitOutputThread = true;
	pollThread.join();
	outputThread.join();

	v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	v4l2Cmd(VIDIOC_STREAMOFF, &type, "failed to stop output streaming");
      
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	v4l2Cmd(VIDIOC_STREAMOFF, &type, "failed to stop capture streaming");
      
	close(encoderFileDescriptor);
	log.info("encoder closed");
}

void H264Encoder::encode(FrameBuffer *frameBuffer, int64_t timestamp_us) {
   
   if (logging::minLevel == DEBUG) {
      log.debug("new frame to encode ( timestamp =", timestamp_us, ")");
   }
   
   int indexOfFreeBuffer;
	{
		std::lock_guard<std::mutex> lock(inputBufferAvailableMutex);
		if (availableInputBuffers.empty()) {
			log.warning("no buffers available to queue codec input -> ignoring frame");
         return;
      }
		indexOfFreeBuffer = availableInputBuffers.front();
		availableInputBuffers.pop();
	}
   
   int  planeCount          = 1;
   auto firstPlane          = frameBuffer->planes()[0];
   auto planeLength         = firstPlane.length;
   auto planeOffset         = firstPlane.offset;
   auto planeSize           = planeLength + planeOffset;
   int  planeFileDiscriptor = firstPlane.fd.get();
   
   v4l2_buffer buf = {};
	v4l2_plane  planes[VIDEO_MAX_PLANES] = {};
   
   buf.type                   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; // Buffer of a multi-planar video output stream
	buf.index                  = indexOfFreeBuffer;                 // Index of the buffer used by the encoder internally
	buf.field                  = V4L2_FIELD_NONE;                   // Images are in progressive format, not interlaced
	buf.memory                 = V4L2_MEMORY_DMABUF;                // The buffer is used for DMA shared buffer I/O
	buf.length                 = planeCount;                        // Number of elements in the planes array
	buf.timestamp.tv_sec       = timestamp_us / 1000000;
	buf.timestamp.tv_usec      = timestamp_us % 1000000;
	buf.m.planes               = planes;
	buf.m.planes[0].m.fd       = planeFileDiscriptor;               // file descriptor associated with a DMABUF buffer,
	buf.m.planes[0].bytesused  = planeLength;                       // The number of bytes occupied by data in the plane (its payload)
	buf.m.planes[0].length     = planeSize;                         // Size in bytes of the plane (not its payload)
   
   v4l2Cmd(VIDIOC_QBUF, &buf, "failed to queue input to codec");
}

/**
 * This task waits for the completion of encoding a NAL. As soon as a NAL
 * is available it moves one input buffer back to the queue of available 
 * buffers and dequeues the output buffer (containing the NAL).
 * The NAL gets put into another queue processed by the outputThreadTask.
 * This decouples the consumption of the JPEG from moving around buffers.
 */
void H264Encoder::pollThreadTask() {
	while (true) {
      pollfd p = { encoderFileDescriptor, POLLIN, 0 };
		// Wait for data to read (POLLIN) from the encoder device and timeout
      // after 200 ms.
      //
      // returnValue >  0 number of elements in the pollfds whose revents 
      //                  fields have been set to POLLIN
      //             =  0 time out
      //             = -1 error
		int returnValue = poll(&p, 1, 200);
		{
			std::lock_guard<std::mutex> lock(inputBufferAvailableMutex);
			if (quitPollThread && availableInputBuffers.size() == H264_INPUT_BUFFER_COUNT) {
				break;
         }
		}
		if (returnValue == -1) {
			if (errno == EINTR) {
				continue;
         }
			throw std::runtime_error("unexpected errno " + std::to_string(errno) + " from poll");
		}
		if (p.revents & POLLIN) {  // H.264 NAL ready to provide to consumer/callback
			v4l2_buffer buf = {};
			v4l2_plane planes[VIDEO_MAX_PLANES] = {};
         
			buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
			buf.memory   = V4L2_MEMORY_DMABUF;
			buf.length   = 1;
			buf.m.planes = planes;
         
         v4l2Cmd(VIDIOC_DQBUF, &buf, "failed to dequeue input buffer"); 
         {
            std::lock_guard<std::mutex> lock(inputBufferAvailableMutex);
            availableInputBuffers.push(buf.index);
			}

			buf = {};
			memset(planes, 0, sizeof(planes));
         
			buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			buf.memory   = V4L2_MEMORY_MMAP;
			buf.length   = 1;
			buf.m.planes = planes;
         
			v4l2Cmd(VIDIOC_DQBUF, &buf, "failed to dequeue output buffer");
         int64_t timestamp_us = (buf.timestamp.tv_sec * (int64_t)1000000) + buf.timestamp.tv_usec;
         H264Nal nal = { outputBufferData[buf.index],
                        buf.m.planes[0].bytesused,
                        buf.m.planes[0].length,
                        buf.index,
                        !!(buf.flags & V4L2_BUF_FLAG_KEYFRAME),
                        timestamp_us };
         std::lock_guard<std::mutex> lock(nalsReadyMutex);
         nalsReadyToConsume.push(nal);
         nalsReadyCondition.notify_one();
		}
	}
}

/**
 * This task waits for NALs enqueued by pollThreadTask and provides
 * them to the consumer/callback. After consumption the output buffer
 * gets enqueue again.
 */
void H264Encoder::outputThreadTask() {
	H264Nal nal;
	while (true) {
		{
			std::unique_lock<std::mutex> lock(nalsReadyMutex);
			while (true) {
				if (quitOutputThread && nalsReadyToConsume.empty()) {
					return;
            }
				if (!nalsReadyToConsume.empty()) {
					nal = nalsReadyToConsume.front();
					nalsReadyToConsume.pop();
					break;
				} else {
					nalsReadyCondition.wait_for(lock, 200ms);
            }
			}
		}

      if (outputReadyCallback) {
         outputReadyCallback(nal.mem, nal.bytes_used, nal.timestamp_us, nal.keyframe);
		}
      
      v4l2_buffer buf = {};
		v4l2_plane planes[VIDEO_MAX_PLANES] = {};
      
		buf.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory                 = V4L2_MEMORY_MMAP;
		buf.index                  = nal.index;
		buf.length                 = 1;
		buf.m.planes               = planes;
		buf.m.planes[0].bytesused  = 0;
		buf.m.planes[0].length     = nal.length;
      
		v4l2Cmd(VIDIOC_QBUF, &buf, "failed to re-queue encoded buffer");
	}
}
