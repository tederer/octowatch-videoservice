#include <algorithm>
#include <sstream>
#include <utility>

#include <libcamera/controls.h>

#include "CameraCapabilities.h"
#include "StringUtils.h"

#define TYPE_BOOL    "boolean"
#define TYPE_BYTE    "byte"
#define TYPE_INT32   "int32"
#define TYPE_INT64   "int64"
#define TYPE_FLOAT   "float"

using capabilities::Capability;
using capabilities::CameraCapabilities;
using libcamera::ControlInfoMap;
using libcamera::ControlList;
using libcamera::ControlType;
using logging::Logger;

// type definitions: https://libcamera.org/api-html/controls_8h_source.html
const std::map<libcamera::ControlType, std::string> CameraCapabilities::CONTROL_TYPE_MAPPING {
   {ControlType::ControlTypeBool 	 , TYPE_BOOL},
   {ControlType::ControlTypeByte 	 , TYPE_BYTE},
   {ControlType::ControlTypeInteger32, TYPE_INT32},
   {ControlType::ControlTypeInteger64, TYPE_INT64},
   {ControlType::ControlTypeFloat 	 , TYPE_FLOAT},
};

Capability::Capability(const libcamera::ControlId*   controlId, 
                       const libcamera::ControlInfo& controlInfo,
                       SetControlsFunction           setControlsFunction,
                       Logger&                       log)
   :  controlId(controlId->id()), 
      controlName(controlId->name()), 
      controlType(""),
      setControls(setControlsFunction),
      log(log) {
         auto searchResult = CameraCapabilities::CONTROL_TYPE_MAPPING.find(controlId->type());
         if (searchResult != CameraCapabilities::CONTROL_TYPE_MAPPING.end()) {
            controlType = searchResult->second;
         } else {
            std::ostringstream typeText;
            typeText << "raw-";
            typeText << controlId->type();
            controlType = typeText.str();
         }
         
         if (controlId->type() == ControlType::ControlTypeBool) {
            minimumValue = (float)controlInfo.min().get<bool>(); 
            maximumValue = (float)controlInfo.max().get<bool>(); 
            defaultValue = (float)controlInfo.def().get<bool>(); 
            currentValue = (float)controlInfo.def().get<bool>(); 
            return;
         }
         
         if (controlId->type() == ControlType::ControlTypeByte) {
            minimumValue = (float)controlInfo.min().get<uint8_t>(); 
            maximumValue = (float)controlInfo.max().get<uint8_t>(); 
            defaultValue = (float)controlInfo.def().get<uint8_t>(); 
            currentValue = (float)controlInfo.def().get<uint8_t>(); 
            return;
         }
         
         if (controlId->type() == ControlType::ControlTypeInteger32) {
            minimumValue = (float)controlInfo.min().get<int32_t>(); 
            maximumValue = (float)controlInfo.max().get<int32_t>(); 
            defaultValue = (float)controlInfo.def().get<int32_t>(); 
            currentValue = (float)controlInfo.def().get<int32_t>(); 
            return;
         }
         
         if (controlId->type() == ControlType::ControlTypeInteger64) {
            minimumValue = (float)controlInfo.min().get<int64_t>(); 
            maximumValue = (float)controlInfo.max().get<int64_t>(); 
            defaultValue = (float)controlInfo.def().get<int64_t>(); 
            currentValue = (float)controlInfo.def().get<int64_t>(); 
            return;
         }
         
         if (controlId->type() == ControlType::ControlTypeFloat) {
            minimumValue = controlInfo.min().get<float>(); 
            maximumValue = controlInfo.max().get<float>(); 
            defaultValue = controlInfo.def().get<float>(); 
            currentValue = controlInfo.def().get<float>(); 
            return;
         }
         
         log.error("unsupported control type:", controlType);
      }
   
bool Capability::setCurrentValue(float newValue) {
   bool result = false;
   
   if ((newValue < minimumValue) || (newValue > maximumValue)) {
      log.error("new value", newValue, "for capability", controlName, 
      "is out of range [", minimumValue, ",", maximumValue, "]");
   } else {
      log.debug("new value for", controlName, ":", newValue);
      currentValue = newValue;
      result = setControl();
   }
   
   return result;
}
      
const std::string&   Capability::getName()          const { return controlName; }
const std::string&   Capability::getType()          const { return controlType; }
float                Capability::getMinimumValue()  const { return minimumValue; }
float                Capability::getMaximumValue()  const { return maximumValue; }
float                Capability::getDefaultValue()  const { return defaultValue; }
float                Capability::getCurrentValue()  const { return currentValue; }
      
std::string Capability::toString() const {
   std::ostringstream message;
   message  << controlName << " [type=" << controlType << ", minimum=" << minimumValue 
            << ", maximum=" << maximumValue << ", default=" << defaultValue << ", current=" 
            << currentValue << "]";
   return message.str();
}
   
bool Capability::setControl() {
   log.info("setting", controlName, "to", currentValue);
   std::unique_ptr<ControlList> controls(new ControlList());
   bool result = true;
   
   if (controlType == TYPE_BOOL) {
      controls->set(controlId, (bool)currentValue);
   }
   if (controlType == TYPE_BYTE) {
      controls->set(controlId, (uint8_t)currentValue);
   }
   if (controlType == TYPE_INT32) {
      controls->set(controlId, (int32_t)currentValue);
   }
   if (controlType == TYPE_INT64) {
      controls->set(controlId, (int64_t)currentValue);
   }
   if (controlType == TYPE_FLOAT) {
      controls->set(controlId, currentValue);
   }
   
   if (controls->size() != 1) {
      log.error("failed to set value");
      result = false;
   }
   
   setControls(std::move(controls));
   
   return result;
}

CameraCapabilities::CameraCapabilities(const ControlInfoMap& controls, SetControlsFunction setControlsFunction)
   : log("CameraCapablities") {
     
   for (auto tuple : controls) {
      const libcamera::ControlId* controlId   = tuple.first;
      libcamera::ControlInfo      controlInfo = tuple.second;
      
      if (CONTROL_TYPE_MAPPING.find(controlId->type()) != CONTROL_TYPE_MAPPING.end()) {
         
         if (controlInfo.min().isNone() || controlInfo.max().isNone() 
                                        || controlInfo.def().isNone()) {
            log.warning(controlId->name(), 
               "does not support required values -> ignoring it");
            continue;
         }
      
         auto insertedPair = capabilities.emplace(std::make_pair(controlId->name(), 
                                    Capability(controlId, controlInfo, setControlsFunction, log)));
         log.info("available capability:", insertedPair.first->second.toString());
         setValue(controlId->name(), insertedPair.first->second.getDefaultValue(), false);
      }
   }
}
      
void CameraCapabilities::setListener(Listener& newlistener) {
   listener = newlistener;
   notifyCapabilitiesListener();
   notifyValueListener();
}
      
bool CameraCapabilities::setValue(const std::string& controlName, float value, bool notifyListener) {
   bool        valueSet = false;
   std::string expected = utils::String::toLowerCase(controlName);
   
   auto iterator = std::find_if(capabilities.begin(), capabilities.end(), 
      [&expected](auto iterator) {
         std::string actual = utils::String::toLowerCase(iterator.first);
         return expected == actual;
      });
   
   if (iterator == capabilities.end()) {
      log.error("cannot set value of not existing capability", controlName);
   } else {
      valueSet = iterator->second.setCurrentValue(value);
      if (valueSet && notifyListener) {
         notifyValueListener();
      }
   }
   return valueSet;
}
   
void CameraCapabilities::notifyCapabilitiesListener() {
   std::map<std::string, Properties> result;
   for (auto tuple : capabilities) {
      std::string type    = tuple.second.getType();
      float minimumValue  = tuple.second.getMinimumValue();
      float maximumValue  = tuple.second.getMaximumValue();
      float defaultValue  = tuple.second.getDefaultValue();
      result[tuple.first] = Properties(type, minimumValue, maximumValue, defaultValue);
   }
   listener.get().onCapabilitiesChanged(result);
}
      
void CameraCapabilities::notifyValueListener() {
   std::map<std::string, float> result;
   for (auto tuple : capabilities) {
      result[tuple.first] = tuple.second.getCurrentValue();
   }
   listener.get().onCurrentValuesChanged(result);
}
