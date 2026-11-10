#pragma once

#ifdef WITH_MQTT_UPLINK

#include <Arduino.h>
#include <helpers/IdentityStore.h>
#include <helpers/mqtt/NetworkStateProvider.h>
#include <helpers/mqtt/MQTTPrefs.h>

#if defined(ESP_PLATFORM)
  #include <WiFi.h>
#endif

// WiFi station + SNTP management for the MQTT uplink. Credentials and power
// save mode live in MQTTPrefs (pointer set via setConfig, re-read live so
// `set wifi.*` commands take effect). The prefs struct is owned (and persisted)
// by MQTTUplink; WifiLink only mutates it in memory on set commands. All
// methods are no-ops off ESP32.
class WifiLink : public NetworkStateProvider {
public:
  WifiLink();

  void setConfig(MQTTPrefs* prefs) { _prefs = prefs; }
  void loop(bool network_required);
  void reconnectWifi();

  bool setWifiPowerSave(const char* mode);
  const char* getWifiPowerSave() const;
  // Mutate the (shared) MQTTPrefs wifi credentials and reconnect. Persistence
  // is the caller's job (mqtt.savePrefs()).
  bool setWifiSSID(const char* ssid);
  bool setWifiPassword(const char* pwd);
  const char* getWifiSSID() const;
  void formatWifiStatusReply(char* reply, size_t reply_size) const;

  bool isWifiConnected() const override;
  bool hasTimeSync() const override;
  void requestWifiRejoin() override { reconnectWifi(); }

private:
#if defined(ESP_PLATFORM)
  static uint8_t toEspPowerSave(uint8_t mode);
  static const char* getPowerSaveLabel(uint8_t mode);
  void ensureWifi(bool network_required);
  void updateTimeSync();
#endif

  MQTTPrefs* _prefs;  // owned + persisted by MQTTUplink; mutated in memory by set wifi.* commands
  bool _wifi_started;
  bool _sntp_started;
  bool _have_time_sync;
  unsigned long _last_wifi_attempt;
  time_t _last_time_sync;
};

#endif
