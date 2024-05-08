
#include "Logging.h"

unsigned int logging::minLevel = WARNING;

std::mutex logging::mutex = std::mutex();