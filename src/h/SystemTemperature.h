#ifndef TEMPERATURE_LIMIT_H
#define TEMPERATURE_LIMIT_H

#include <filesystem>
#include <functional>
#include <memory>
#include <thread>

#include "Logging.h"

   
/**
 * Checks periodically if the temperature in the camera is below the maxmimum temperature.
 */
class SystemTemperature {
   public:
      SystemTemperature();

      ~SystemTemperature();

      void start(std::function<void(bool)> onTooHighCallback);

   private:
      logging::Logger              log;
      std::unique_ptr<std::thread> thread;
      bool                         tooHigh;
      bool                         disposing;
      bool                         isFirstPolling;
      std::filesystem::path        semaphoreFile;
};

#endif
