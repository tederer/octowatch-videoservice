#ifndef CAMERACAPABILITIES_H
#define CAMERACAPABILITIES_H

#include <functional>
#include <list>
#include <map>
#include <memory>

#include "libcamera/controls.h"
#include "libcamera/control_ids.h"

#include "Logging.h"

typedef std::function<void(std::unique_ptr<libcamera::ControlList>)> SetControlsFunction;

namespace capabilities {
   
   class Capability {
      public:
         Capability(const libcamera::ControlId*   controlId,
                    const libcamera::ControlInfo& controlInfo,
                    SetControlsFunction           setControlsFunction,
                    logging::Logger&              log);
         
         bool setCurrentValue(float newValue);
         
         const std::string& getName() const;
         const std::string& getType() const;
         float getMinimumValue() const;
         float getMaximumValue() const;
         float getDefaultValue() const;
         float getCurrentValue() const;
         
         std::string toString() const;
         
      private:
         bool setControl();
         
         unsigned int        controlId;
         std::string         controlName;
         std::string         controlType;
         float               minimumValue;
         float               maximumValue;
         float               defaultValue;
         float               currentValue;
         SetControlsFunction setControls;
         logging::Logger&    log;
   };
   
   class CameraCapabilities {
      public:
         class Properties {
            public:
               Properties( const std::string& type, 
                           float              minimumValue, 
                           float              maximumValue, 
                           float              defaultValue)
                  : type(type), 
                    minimumValue(minimumValue),
                    maximumValue(maximumValue), 
                    defaultValue(defaultValue) {}
               
               Properties() : type(std::string("n.a.")) {}
               
               std::string type;
               float minimumValue;
               float maximumValue;
               float defaultValue;
         };
         
         class Listener {
            public:
               virtual void onCapabilitiesChanged(std::map<std::string, Properties>& capabilities) = 0;
               virtual void onCurrentValuesChanged(std::map<std::string, float> currentValues) = 0;
         };
         
         static const std::map<libcamera::ControlType, std::string> CONTROL_TYPE_MAPPING;
         
         CameraCapabilities(const libcamera::ControlInfoMap& controls, SetControlsFunction setControlsFunction);
         
         void setListener(Listener& listener);
         
         bool setValue(const std::string& controlName, float value, bool notifyListener = true);
         
      private:
         class NoopListener: public Listener {
            public:
               void onCapabilitiesChanged(std::map<std::string, Properties>& /*capabilities*/) override {};
               void onCurrentValuesChanged(std::map<std::string, float> /*currentValues*/) override {};
         };
         
         void notifyCapabilitiesListener();
         
         void notifyValueListener();
         
         logging::Logger                   log;
         NoopListener                      noopListener = NoopListener();
         std::reference_wrapper<Listener>  listener{noopListener};
         std::map<std::string, Capability> capabilities;
   };
}
#endif
