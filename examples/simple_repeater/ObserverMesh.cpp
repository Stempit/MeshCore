#include "ObserverMesh.h"

#if defined(WITH_MQTT_UPLINK)

void ObserverMesh::begin(FILESYSTEM* fs) {
  MyMesh::begin(fs);

  mqtt.setNodeNameSource(getNodePrefs()->node_name);
  mqtt.setFirmwareVersion(FIRMWARE_VERSION);
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
  mqtt.publishPacket(*pkt, true, (int)_radio->getLastRSSI(), _radio->getLastSNR());
#endif
}

#endif
