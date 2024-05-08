#ifndef DMAHEAP_H
#define DMAHEAP_H

#include <stddef.h>

#include "libcamera/base/unique_fd.h"

#include "Logging.h"

class DmaHeap {
   public:
      DmaHeap();
      
      libcamera::UniqueFD alloc(const char *name, std::size_t size);

   private:
      logging::Logger     log;
      bool                initialized;
      libcamera::UniqueFD fileDescriptor;
};

#endif
