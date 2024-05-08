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

int H264Encoder::get_v4l2_colorspace(std::optional<ColorSpace> const &cs) {
	if (cs == ColorSpace::Rec709)
		return V4L2_COLORSPACE_REC709;
	else if (cs == ColorSpace::Smpte170m)
		return V4L2_COLORSPACE_SMPTE170M;

	return V4L2_COLORSPACE_SMPTE170M;
}

bool H264Encoder::applyDeviceParam(unsigned long ctl, void *arg) {
	int returnValue;
   int remainingIterations = 10;
   
	do {
		returnValue = ioctl(encoderFileDescriptor, ctl, arg);
	} while ((returnValue == -1) && (errno == EINTR) && (remainingIterations-- > 0));
   
   if (returnValue == -1) {
      std::ostringstream logMessage;
      logMessage << "ioctl failed: ";
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
      log.error(logMessage.str());
   }
   
	return (returnValue == 0);
}

void H264Encoder::setOutputReadyCallback(OutputReadyCallback callback) {
   outputReadyCallback = callback; 
}

H264Encoder::H264Encoder(StreamConfiguration const &streamConfig)
	: log("H264Encoder"), abortPoll_(false), abortOutput_(false) {
      
   std::string deviceName = "/dev/video11";
	encoderFileDescriptor = open(deviceName.c_str(), O_RDWR, 0);
	if (encoderFileDescriptor < 0) {
		throw std::runtime_error("failed to open V4L2 H264 encoder");
   }
	log.info("Opened H264Encoder on", deviceName, "as fd", encoderFileDescriptor);

	v4l2_control ctrl = {};
	
   ctrl.id = V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER;
   ctrl.value = 1;
   if (!applyDeviceParam(VIDIOC_S_CTRL, &ctrl)) {
      throw std::runtime_error("failed to set inline headers");
   }
	
	v4l2_format fmt = {};
	fmt.type                                  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.width                      = streamConfig.size.width;
	fmt.fmt.pix_mp.height                     = streamConfig.size.height;
	fmt.fmt.pix_mp.pixelformat                = V4L2_PIX_FMT_YUV420;
	fmt.fmt.pix_mp.plane_fmt[0].bytesperline  = streamConfig.stride;
	fmt.fmt.pix_mp.field                      = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.colorspace                 = get_v4l2_colorspace(streamConfig.colorSpace);
	fmt.fmt.pix_mp.num_planes                 = 1;
	
   if (!applyDeviceParam(VIDIOC_S_FMT, &fmt)) {
		throw std::runtime_error("failed to set output format");
   }
   
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
   
	if (!applyDeviceParam(VIDIOC_S_FMT, &fmt)) {
		throw std::runtime_error("failed to set capture format");
   }
   
	double framerate            = 30;
   struct v4l2_streamparm parm = {};
   parm.type                                 = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
   parm.parm.output.timeperframe.numerator   = 90000.0 / framerate;
   parm.parm.output.timeperframe.denominator = 90000;
   
   if (!applyDeviceParam(VIDIOC_S_PARM, &parm)) {
      throw std::runtime_error("failed to set streamparm");
   }

	v4l2_requestbuffers reqbufs = {};
	reqbufs.count               = NUM_OUTPUT_BUFFERS;
	reqbufs.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbufs.memory              = V4L2_MEMORY_DMABUF;
   
	if (!applyDeviceParam(VIDIOC_REQBUFS, &reqbufs)) {
		throw std::runtime_error("request for output buffers failed");
   }
	log.info("Got", reqbufs.count, "output buffers");

	for (unsigned int i = 0; i < reqbufs.count; i++) {
		availableInputBuffers.push(i);
   }
   
	reqbufs        = {};
	reqbufs.count  = NUM_CAPTURE_BUFFERS;
	reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbufs.memory = V4L2_MEMORY_MMAP;
	if (!applyDeviceParam(VIDIOC_REQBUFS, &reqbufs)) {
		throw std::runtime_error("request for capture buffers failed");
   }
	log.info("Got", reqbufs.count, "capture buffers");
	num_capture_buffers_ = reqbufs.count;

	for (unsigned int i = 0; i < reqbufs.count; i++) {
		v4l2_plane planes[VIDEO_MAX_PLANES];
		v4l2_buffer buffer = {};
		buffer.type       = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buffer.memory     = V4L2_MEMORY_MMAP;
		buffer.index      = i;
		buffer.length     = 1;
		buffer.m.planes   = planes;
      
		if (!applyDeviceParam(VIDIOC_QUERYBUF, &buffer)) {
			throw std::runtime_error("failed to capture query buffer " + std::to_string(i));
      }
		buffers_[i].mem = mmap(0, buffer.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, encoderFileDescriptor,
							   buffer.m.planes[0].m.mem_offset);
		if (buffers_[i].mem == MAP_FAILED) {
			throw std::runtime_error("failed to mmap capture buffer " + std::to_string(i));
      }
		buffers_[i].size = buffer.m.planes[0].length;
		
      if (!applyDeviceParam(VIDIOC_QBUF, &buffer)) {
			throw std::runtime_error("failed to queue capture buffer " + std::to_string(i));
      }
	}

	v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (!applyDeviceParam(VIDIOC_STREAMON, &type)) {
		throw std::runtime_error("failed to start output streaming");
   }
   
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (!applyDeviceParam(VIDIOC_STREAMON, &type)) {
		throw std::runtime_error("failed to start capture streaming");
   }
   
	log.info("Codec streaming started");

	output_thread_ = std::thread(&H264Encoder::outputThread, this);
	poll_thread_   = std::thread(&H264Encoder::pollThread,   this);
}

H264Encoder::~H264Encoder() {
	abortPoll_ = true;
	poll_thread_.join();
	abortOutput_ = true;
	output_thread_.join();

	v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
   
	if (!applyDeviceParam(VIDIOC_STREAMOFF, &type)) {
		log.error("Failed to stop output streaming");
   }
   
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
   
	if (!applyDeviceParam(VIDIOC_STREAMOFF, &type)) {
		log.error("Failed to stop capture streaming");
   }
   
	v4l2_requestbuffers reqbufs = {};
	reqbufs.count               = 0;
	reqbufs.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbufs.memory              = V4L2_MEMORY_DMABUF;
   
	if (!applyDeviceParam(VIDIOC_REQBUFS, &reqbufs)) {
		log.error("Request to free output buffers failed");
   }
   
	for (int i = 0; i < num_capture_buffers_; i++) {
		if (munmap(buffers_[i].mem, buffers_[i].size) < 0) {
			log.error("Failed to unmap buffer");
      }
   }
	reqbufs        = {};
	reqbufs.count  = 0;
	reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbufs.memory = V4L2_MEMORY_MMAP;
   
	if (!applyDeviceParam(VIDIOC_REQBUFS, &reqbufs)) {
		log.error("Request to free capture buffers failed");
   }
   
	close(encoderFileDescriptor);
	log.info("H264Encoder closed");
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
   
   // VIDIOC_QBUF = enqueue a buffer in the driver’s incoming queue
	if (!applyDeviceParam(VIDIOC_QBUF, &buf)) {
		throw std::runtime_error("failed to queue input to codec");
   }
}

void H264Encoder::pollThread() {
	while (true)
	{
      pollfd p = { encoderFileDescriptor, POLLIN, 0 };
		// wait for data to read (POLLIN) on one file description and timeout
      // after 200 ms.
      // returnValue > 0  number of elements in the pollfds whose revents 
      //                  fields have been set to a nonzero value
      //             = 0  time out
      //             = -1 error
		int returnValue = poll(&p, 1, 200);
		{
			std::lock_guard<std::mutex> lock(inputBufferAvailableMutex);
			if (abortPoll_ && availableInputBuffers.size() == NUM_OUTPUT_BUFFERS) {
				break;
         }
		}
		if (returnValue == -1)
		{
			if (errno == EINTR) {
				continue;
         }
			throw std::runtime_error("unexpected errno " + std::to_string(errno) + " from poll");
		}
		if (p.revents & POLLIN) // data ready to read
		{
			v4l2_buffer buf = {};
			v4l2_plane planes[VIDEO_MAX_PLANES] = {};
         
			buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
			buf.memory   = V4L2_MEMORY_DMABUF;
			buf.length   = 1;
			buf.m.planes = planes;
         
         // VIDIOC_DQBUF = dequeue a buffer from the driver’s outgoing queue
			if (applyDeviceParam(VIDIOC_DQBUF, &buf)) {
				{
					std::lock_guard<std::mutex> lock(inputBufferAvailableMutex);
					availableInputBuffers.push(buf.index);
				}
			}

			buf = {};
			memset(planes, 0, sizeof(planes));
         
			buf.type       = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			buf.memory     = V4L2_MEMORY_MMAP;
			buf.length     = 1;
			buf.m.planes   = planes;
         
			if (applyDeviceParam(VIDIOC_DQBUF, &buf)) {
				// We push this encoded buffer to another thread so that our
				// application can take its time with the data without blocking the
				// encode process.
				int64_t timestamp_us = (buf.timestamp.tv_sec * (int64_t)1000000) + buf.timestamp.tv_usec;
				OutputItem item = { buffers_[buf.index].mem,
									buf.m.planes[0].bytesused,
									buf.m.planes[0].length,
									buf.index,
									!!(buf.flags & V4L2_BUF_FLAG_KEYFRAME),
									timestamp_us };
				std::lock_guard<std::mutex> lock(output_mutex_);
				output_queue_.push(item);
				output_cond_var_.notify_one();
			}
		}
	}
}

void H264Encoder::outputThread() {
	OutputItem item;
	while (true)
	{
		{
			std::unique_lock<std::mutex> lock(output_mutex_);
			while (true) {
				// Must check the abort first, to allow items in the output
				// queue to have a callback.
				if (abortOutput_ && output_queue_.empty()) {
					return;
            }
				if (!output_queue_.empty()) {
					item = output_queue_.front();
					output_queue_.pop();
					break;
				} else {
					output_cond_var_.wait_for(lock, 200ms);
            }
			}
		}

      if (outputReadyCallback) {
         outputReadyCallback(item.mem, item.bytes_used, item.timestamp_us, item.keyframe);
		}
      
      v4l2_buffer buf = {};
		v4l2_plane planes[VIDEO_MAX_PLANES] = {};
      
		buf.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory                 = V4L2_MEMORY_MMAP;
		buf.index                  = item.index;
		buf.length                 = 1;
		buf.m.planes               = planes;
		buf.m.planes[0].bytesused  = 0;
		buf.m.planes[0].length     = item.length;
      
		if (!applyDeviceParam(VIDIOC_QBUF, &buf)) {
			throw std::runtime_error("failed to re-queue encoded buffer");
      }
	}
}
