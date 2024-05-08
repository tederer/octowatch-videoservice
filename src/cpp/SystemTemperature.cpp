
#include <chrono>
#include <sstream>

#include "SystemTemperature.h"

using logging::Logger;

using namespace std::chrono_literals;

SystemTemperature::SystemTemperature() 
   : log("SystemTemperature"), 
     tooHigh(false), 
     disposing(false),
     isFirstPolling(true) {
        
   char* homePath = std::getenv("HOME");
   
   if (!homePath) {
      log.warning("failed to read HOME environment variable");
      semaphoreFile = std::filesystem::path("/home/tux/.temperatureTooHigh");
   } else {
      std::ostringstream path;
      path << homePath << "/.temperatureTooHigh";
      semaphoreFile = std::filesystem::path(path.str());
   }
   
   log.info("semaphore file path:", semaphoreFile);
}

void SystemTemperature::start(std::function<void(bool)> onTooHighCallback) {
   
   thread = std::unique_ptr<std::thread>(new std::thread([this, onTooHighCallback](){
      int iterationCount = 0;
      
      while(!disposing) {
         if ((iterationCount % 10) == 0) {
            bool newTooHigh = std::filesystem::exists(semaphoreFile);
            if (isFirstPolling || (newTooHigh != tooHigh)) {
               tooHigh        = newTooHigh;
               isFirstPolling = false;
               log.info("semaphore file", std::string(tooHigh ? "exists" : "does not exist"));
               onTooHighCallback(tooHigh);
            }
            iterationCount = 0;
         }
         iterationCount++;
         std::this_thread::sleep_for(500ms);
      }
      log.info("left polling loop");
   }));
}

SystemTemperature::~SystemTemperature() {
   disposing = true;
   log.info("waiting for the thread to finish its execution");
   thread->join();
   log.info("thread finished");
}

