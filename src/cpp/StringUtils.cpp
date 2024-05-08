
#include <algorithm>

#include "StringUtils.h"

std::string utils::String::toLowerCase(const std::string& text) {
   std::string result(text.size(), ' ');
   std::transform(text.begin(), text.end(), result.begin(),
                     [](unsigned char c){ return std::tolower(c); });
   return result;
}
