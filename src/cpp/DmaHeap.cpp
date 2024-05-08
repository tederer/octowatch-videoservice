#include "DmaHeap.h"

#include <array>
#include <vector>
#include <errno.h>
#include <fcntl.h>
#include <sstream>

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <unistd.h>

using logging::Logger;

DmaHeap::DmaHeap() : log("DmaHeap"), initialized(false) {
   const std::vector<const char *> heapNames {
      "/dev/dma_heap/vidbuf_cached",
      "/dev/dma_heap/linux,cma"};

	for (const char *name : heapNames) {
		int fd = open(name, O_RDWR | O_CLOEXEC, 0);
		if (fd < 0) {
			log.error("Failed to open", name, ": error code", fd);
			continue;
		}

		fileDescriptor = libcamera::UniqueFD(fd);
		break;
	}

   initialized = fileDescriptor.isValid();
   
	if (!initialized) {
		log.error("failed to initialize heap");
   }
}

libcamera::UniqueFD DmaHeap::alloc(const char *name, std::size_t size) {
   if (!initialized) {
		log.error("cannot allocate heap because it is not initialized");
      return {};
   }
   
	if (!name) {
		log.error("cannot allocate heap because name argument is missing");
      return {};
   }
   
	struct dma_heap_allocation_data allocationData = {};

	allocationData.len      = size;
	allocationData.fd_flags = O_CLOEXEC | O_RDWR;

	if (ioctl(fileDescriptor.get(), DMA_HEAP_IOCTL_ALLOC, &allocationData) < 0) {
		log.error("failed to allocate heap for", name, ": error code", errno);
		return {};
	}

	libcamera::UniqueFD allocFd(allocationData.fd);
   
	if (ioctl(allocFd.get(), DMA_BUF_SET_NAME, name) < 0) {
		log.error("failed to set name", name, "for heap: error code", errno);
		return {};
	}

   std::ostringstream logMessage;
   logMessage << "allocated heap (fileDiscriptor = " << allocFd.get() << ", name = " << name << ", size = " << size << ")";
   
   log.info(logMessage.str());
	return allocFd;
}
