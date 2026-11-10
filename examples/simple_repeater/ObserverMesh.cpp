#include "ObserverMesh.h"

#if defined(WITH_MQTT_UPLINK)

#include <stdlib.h>
#include <string.h>
#include <time.h>

// Epoch of this firmware's build, parsed from __DATE__ "Mmm dd yyyy" and
// __TIME__ "hh:mm:ss" (refreshed by the release script, which touches this
// file before building). Used as a one-shot lower bound for the clock: on
// boards without a battery-backed RTC the clock comes up at the fallback
// baseline (e.g. VolatileRTCClock = 15 May 2024), and we bump it forward to
// the build time so it is as close to "now" as possible until NTP or a mesh
// clock sync corrects it. Never moves the clock backward.
static uint32_t buildEpoch() {
  struct tm t = {};
  static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], 0};
  for (int i = 0; i < 12; ++i) {
    if (strcmp(mon, months[i]) == 0) {
      t.tm_mon = i;
      break;
    }
  }
  t.tm_mday = atoi(__DATE__ + 4);
  t.tm_year = atoi(__DATE__ + 7) - 1900;
  t.tm_hour = atoi(__TIME__);
  t.tm_min = atoi(__TIME__ + 3);
  t.tm_sec = atoi(__TIME__ + 6);
  return (uint32_t)mktime(&t);
}

void ObserverMesh::begin(FILESYSTEM* fs) {
  uint32_t build = buildEpoch();
  uint32_t now = getRTCClock()->getCurrentTime();
  if (now < build) {
    getRTCClock()->setCurrentTime(build);
    Serial.printf("[observer] clock reset detected, seeded with build time\n");
  }

  MyMesh::begin(fs);

  // Build stamp travels in the status JSON's firmware_version field so the
  // server can tell builds apart; the release script touches this file before
  // building so __DATE__/__TIME__ are always fresh.
  mqtt.setFirmwareVersion(FIRMWARE_VERSION " built " __DATE__ " " __TIME__);
  Serial.printf("[observer] firmware built " __DATE__ " " __TIME__ "\n");
  mqtt.setNodeNameSource(getNodePrefs()->node_name);
  mqtt.setBoardModel(board.getManufacturerName());
  mqtt.setNetworkStateProvider(&wifi_link);
  mqtt.begin(fs);
  wifi_link.setConfig(&mqtt.getPrefs());
#if defined(ESP_PLATFORM)
  board.setInhibitSleep(true);  // observer: keep LoRa + WiFi radios alive
#endif
}

void ObserverMesh::loop() {
  MyMesh::loop();

  uint32_t now = millis();
  _uptime_millis += now - _last_millis;
  _last_millis = now;

#if defined(ESP_PLATFORM)
  wifi_link.loop(mqtt.isActive());

  MQTTStatusSnapshot mqtt_status{};
  mqtt_status.battery_mv = (int)board.getBattMilliVolts();
  mqtt_status.uptime_secs = (uint32_t)(_uptime_millis / 1000);
  mqtt_status.error_flags = _err_flags;
  mqtt_status.queue_len = (uint16_t)_mgr->getOutboundTotal();
  mqtt_status.noise_floor = _radio->getNoiseFloor();
  mqtt_status.tx_air_secs = (uint32_t)(getTotalAirTime() / 1000);
  mqtt_status.rx_air_secs = (uint32_t)(getReceiveAirTime() / 1000);
  mqtt_status.recv_errors = radio_driver.getPacketsRecvErrors();
  NodePrefs* prefs = getNodePrefs();
  mqtt_status.radio_freq = prefs->freq;
  mqtt_status.radio_bw = prefs->bw;
  mqtt_status.radio_sf = prefs->sf;
  mqtt_status.radio_cr = prefs->cr;
  mqtt.loop(mqtt_status);
#endif
}

void ObserverMesh::handleCommand(uint32_t sender_timestamp, char* command, char* reply) {
  if (mqtt.handleCliCommand(wifi_link, command, reply)) {
    return;
  }
  MyMesh::handleCommand(sender_timestamp, command, reply);
}

bool ObserverMesh::hasPendingWork() const {
  if (MyMesh::hasPendingWork()) return true;
  return mqtt.isActive();  // observer uplink needs WiFi radio, can't sleep
}

void ObserverMesh::logRx(mesh::Packet* pkt, int len, float score) {
  MyMesh::logRx(pkt, len, score);
#if defined(ESP_PLATFORM)
  mqtt.publishPacket(*pkt, false, (int)_radio->getLastRSSI(), _radio->getLastSNR(), (int)(score * 1000),
                     (int)_radio->getEstAirtimeFor(len));
#endif
}

void ObserverMesh::logTx(mesh::Packet* pkt, int len) {
  MyMesh::logTx(pkt, len);
#if defined(ESP_PLATFORM)
  // SNR/RSSI of a transmission don't exist; the VBart-format convention is to
  // report the last received values so the fields stay present in the JSON.
  mqtt.publishPacket(*pkt, true, (int)_radio->getLastRSSI(), _radio->getLastSNR());
#endif
}

#endif
