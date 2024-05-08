#ifndef LOGGING_H
#define LOGGING_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string.h>

#define LOG_SEPARATOR ";"

// internal log levels
#define DEBUG   1
#define INFO    2
#define WARNING 3
#define ERROR   4
#define OFF     5

namespace logging {
   
   static std::map<unsigned int, std::string> LOG_LEVEL_NAME {
      {DEBUG,   "DEBUG"}, 
      {INFO,    "INFO"}, 
      {WARNING, "WARNING"}, 
      {ERROR,   "ERROR"},
      {OFF,     "OFF"}};

   extern unsigned int minLevel;
   extern std::mutex mutex;
   
   class Logger {
      public:   
         Logger(const char* name) : name(name) {} 

         template<typename... Tail> void debug(Tail... tail) {
             log(DEBUG, tail...);
         }

         template<typename... Tail> void info(Tail... tail) {
             log(INFO, tail...);
         }

         template<typename... Tail> void warning(Tail... tail) {
             log(WARNING, tail...);
         }

         template<typename... Tail> void error(Tail... tail) {
             log(ERROR, tail...);
         }
                
      private:
         std::string timestamp() {
            auto now       = std::chrono::system_clock::now();
            uint64_t value = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
            time_t seconds = value / std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count();
            std::tm info;

            memset(&info, 0x00, sizeof(tm));
            gmtime_r(&seconds, &info);

            std::ostringstream result;
            result.imbue(std::locale("C"));
            result << std::setw(4) << std::setfill('0') << (info.tm_year + 1900) << "-";
            result << std::setw(2) << std::setfill('0') << (info.tm_mon + 1) << "-";
            result << std::setw(2) << std::setfill('0') << info.tm_mday << " ";
            result << std::setw(2) << std::setfill('0') << info.tm_hour << ":";
            result << std::setw(2) << std::setfill('0') << info.tm_min << ":";
            result << std::setw(2) << std::setfill('0') << info.tm_sec << ",";
            result << std::setw(3) << std::setfill('0') << (value / 1000000ULL) % 1000;

            return result.str();
         }

         void logIt(std::ostringstream &outputStream) {
            const std::lock_guard<std::mutex> lock(mutex);
            std::cout << outputStream.str() << std::endl;
         }

         template<typename T, typename... Tail> void logIt(std::ostringstream &outputStream, T head, Tail... tail) {
            outputStream << head << " ";
            logIt(outputStream, tail...);
         } 

         template<typename T, typename... Tail> void log(unsigned int level, T head, Tail... tail) {
            if (level >= minLevel) {
               std::ostringstream outputStream;
               outputStream << timestamp() << LOG_SEPARATOR << LOG_LEVEL_NAME[level] << LOG_SEPARATOR << name << LOG_SEPARATOR;
               logIt(outputStream, head, tail...);
            }
         } 
         
         std::string name;
   };
}
#endif
