#pragma once

#ifdef WITH_MQTT_UPLINK

// Minimal seam so MQTTUplink stays WiFi-agnostic: the repeater wires it to a
// WifiLink instance (see helpers/mqtt/WifiLink.h).
class NetworkStateProvider {
public:
  virtual ~NetworkStateProvider() {}
  virtual bool isWifiConnected() const = 0;
  virtual bool hasTimeSync() const = 0;
};

#endif
