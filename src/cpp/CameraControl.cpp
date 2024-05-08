
#include <regex>
#include <sstream>
#include <string>

#include "CameraControl.h"
#include "StringUtils.h"

using capabilities::CameraCapabilities;

CameraControl::CameraControl(Camera &camera) : 
   log("CameraControl"),
   camera(camera),
   capabilitiesMessage(std::optional<std::string>()),
   currentValuesMessage(std::optional<std::string>()),
   capabilitiesMessageNotYetSent(true),
   remoteControlConnected(false),
   remoteControl(remotecontrol::RemoteControl())
   {}

void CameraControl::start() {
   remoteControl.start(*this);
   camera.setCapabilitiesListener(*this);
}

std::string CameraControl::encodeCapabilities(std::map<std::string, CameraCapabilities::Properties>& capabilities) const {
   std::ostringstream json;
   json << "{\"type\":\"capabilities\",\"content\":{";
   unsigned int addedCapabilities = 0;
   for (auto t1 : capabilities) {
      json << "\"" << t1.first << "\":{";
      json << "\"type\":\""    << t1.second.type << "\",";
      json << "\"minimum\":"   << t1.second.minimumValue << ",";
      json << "\"maximum\":"   << t1.second.maximumValue << ",";
      json << "\"default\":"   << t1.second.defaultValue;
      json << "}";
      addedCapabilities++;
      if (addedCapabilities < capabilities.size()) {
         json << ",";
      }
   }
   json << "}}";
   return json.str();
}

std::string CameraControl::encodeCurrentValues(const std::map<std::string, float>& currentValues) const {
   std::ostringstream json;
   json << "{\"type\":\"currentValues\",\"content\":{";
   unsigned int addedValues = 0;
   for (auto t1 : currentValues) {
      json << "\"" << t1.first << "\":" << t1.second;
      addedValues++;
      if (addedValues < currentValues.size()) {
         json << ",";
      }
   }
   json << "}}";
   return json.str();
}

std::string CameraControl::error(const std::string& message) {
   std::ostringstream json;
   json << "{\"type\":\"error\", \"content\":{\"message\":\"" << message << "\"}}";
   return json.str();
}

void CameraControl::sendCapabilitiesMessage() {
   if (remoteControlConnected && capabilitiesMessageNotYetSent && capabilitiesMessage.has_value()) {
      capabilitiesMessageNotYetSent = false;
      remoteControl.asyncSend(capabilitiesMessage.value());
   }
}
      
void CameraControl::sendCurrentValuesMessage() {
   if (remoteControlConnected && currentValuesMessage.has_value()) {
      remoteControl.asyncSend(currentValuesMessage.value());
   }
}
      
void CameraControl::onCapabilitiesChanged(std::map<std::string, 
                        CameraCapabilities::Properties>& capabilities) {
   capabilitiesMessage = encodeCapabilities(capabilities);
   sendCapabilitiesMessage();
}

void CameraControl::onCurrentValuesChanged(std::map<std::string, float> currentValues) {
   std::ostringstream message;
   message << "current values: ";
   for (auto tuple : currentValues) {
      message << tuple.first << "=" << tuple.second << " ";
   }
   log.info(message.str());
   currentValuesMessage = encodeCurrentValues(currentValues);
   sendCurrentValuesMessage();
}

void CameraControl::onNewConnection() {
   remoteControlConnected = true;
   sendCapabilitiesMessage();
   sendCurrentValuesMessage();
}

void CameraControl::onConnectionClosed() {
   capabilitiesMessageNotYetSent  = true;
   remoteControlConnected = false;
}

// command example: {"type":"setControl","content":{"control":"brightness","value":4.7}}
void CameraControl::onCommandReceived(const std::string& command) {
   const std::regex regex("\\{\"type\":\"([a-zA-Z0-9]+)\",\"content\":\\{\"control\":\"([a-zA-Z0-9]+)\",\"value\":(-?[0-9]+(\\.[0-9]+)?)\\}\\}");
   std::smatch captureGroups;
   std::string commandWithoutWhitespaces = command;
   
   commandWithoutWhitespaces.erase(std::remove_if(commandWithoutWhitespaces.begin(),
      commandWithoutWhitespaces.end(), [](auto t) {
         return std::isspace(t);
      }), commandWithoutWhitespaces.end());
    
   if (std::regex_match(commandWithoutWhitespaces, captureGroups, regex)) {
      if (captureGroups.size() >= 3) {
         std::string control = captureGroups[2].str();
         float       value   = std::stof(captureGroups[3].str());
         if (!camera.setControl(control, value)) {
            log.error("failed to execute command:", command);
            remoteControl.asyncSend(error("failed to execute command: " + command));
         }
      }
   } else {
      log.error("ignoring unknown command (regex does not match):", command);
      remoteControl.asyncSend(error("unknown command: " + command));
   }
}