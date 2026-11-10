#pragma once

#if defined(WITH_MQTT_UPLINK)

#include "MyMesh.h"

#include <helpers/mqtt/MQTTUplink.h>
#include <helpers/mqtt/WifiLink.h>

// Observer flavour of the repeater: adds the meshcoretel MQTT uplink on top of
// the stock MyMesh without modifying upstream example code. main.cpp
// instantiates this class instead of MyMesh when WITH_MQTT_UPLINK is defined,
// so upstream example files stay pristine and rebases stay trivial.
//
// Dispatch notes:
// - logRx/logTx are Dispatcher virtuals; overriding them here is seen by the
//   Dispatcher core.
// - handleCommand is declared virtual in MyMesh.h (the only upstream edit
//   besides main.cpp), so the remote-admin CLI path also reaches this class.
// - begin/loop/hasPendingWork are called on the concrete object in main.cpp,
//   so plain shadowing is sufficient.
class ObserverMesh : public MyMesh {
  WifiLink wifi_link;
  MQTTUplink mqtt;
  uint32_t _last_millis;
  uint64_t _uptime_millis;

public:
  ObserverMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng,
               mesh::RTCClock& rtc, mesh::MeshTables& tables)
      : MyMesh(board, radio, ms, rng, rtc, tables), wifi_link(), mqtt(rtc, self_id), _last_millis(0),
        _uptime_millis(0) {
  }

  void begin(FILESYSTEM* fs);
  void loop();
  void handleCommand(uint32_t sender_timestamp, char* command, char* reply) override;
  bool hasPendingWork() const;

protected:
  void logRx(mesh::Packet* pkt, int len, float score) override;
  void logTx(mesh::Packet* pkt, int len) override;
};

#endif
