#pragma once

#ifdef WITH_MQTT_UPLINK

#include <helpers/IdentityStore.h>
#include <stdint.h>

#ifndef MQTT_DEFAULT_IATA
  #define MQTT_DEFAULT_IATA "UNSET"
#endif

#ifndef MQTT_UNSET_IATA
  #define MQTT_UNSET_IATA "UNSET"
#endif

struct MQTTPrefs {
  uint32_t magic;
  uint8_t enabled_mask;       // bit 0 = meshcoretel (default on), 1 = letsmesh-eu, 2 = letsmesh-us
  uint8_t packets_enabled;
  uint8_t raw_enabled;
  uint8_t status_enabled;
  uint8_t tx_enabled;
  uint8_t wifi_powersave;     // 0 = none, 1 = min, 2 = max
  uint8_t reserved[2];
  uint32_t status_interval_ms;
  char iata[8];
  char wifi_ssid[33];
  char wifi_pwd[65];
  char owner_public_key[65];  // optional LetsMesh JWT claim
  char owner_email[96];       // optional LetsMesh JWT claim
};

class MQTTPrefsStore {
public:
  static void setDefaults(MQTTPrefs& prefs);
  static bool load(FILESYSTEM* fs, MQTTPrefs& prefs);
  static bool save(FILESYSTEM* fs, const MQTTPrefs& prefs);

private:
  static constexpr uint32_t kMagic = 0x4D515454;
  static constexpr const char* kFilename = "/mqtt_prefs";
};

#endif
