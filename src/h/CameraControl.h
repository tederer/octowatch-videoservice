#ifndef CAMERACONTROL_H
#define CAMERACONTROL_H

#include "Camera.h"
#include "CameraCapabilities.h"
#include "Logging.h"
#include "RemoteControl.h"

class CameraControl :   public capabilities::CameraCapabilities::Listener, 
                        public remotecontrol::RemoteControl::Listener {
   public:
      CameraControl(Camera &camera);
   
      void start();
      
      // CameraCapabilities listener callbacks
      void onCapabilitiesChanged(std::map<std::string, 
            capabilities::CameraCapabilities::Properties>& capabilities) override;
      
      void onCurrentValuesChanged(std::map<std::string, float> currentValues) override;

      // RemoteControl listener callbacks
      void onNewConnection() override;
      
      void onConnectionClosed() override;
      
      void onCommandReceived(const std::string& command) override;
      
   private:
      void sendCapabilitiesMessage();
      
      void sendCurrentValuesMessage();
      
      std::string encodeCapabilities(std::map<std::string, 
            capabilities::CameraCapabilities::Properties>& capabilities) const;
      
      std::string encodeCurrentValues(const std::map<std::string, float>& currentValues) const;
      
      std::string error(const std::string& message);
      
      logging::Logger                  log;
      Camera &                         camera;
      std::optional<std::string>       capabilitiesMessage;
      std::optional<std::string>       currentValuesMessage;
      bool                             capabilitiesMessageNotYetSent;
      bool                             remoteControlConnected;
      remotecontrol::RemoteControl     remoteControl;
};

#endif
