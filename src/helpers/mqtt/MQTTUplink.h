#pragma once

#ifdef WITH_MQTT_UPLINK

#include <Mesh.h>
#include <helpers/mqtt/NetworkStateProvider.h>
#include <target.h>

#include "JWTHelper.h"
#include "MQTTPrefs.h"

class WifiLink;

#if defined(ESP_PLATFORM)
#include <mqtt_client.h>
#endif

struct MQTTStatusSnapshot {
  int battery_mv;
  uint32_t uptime_secs;
  uint16_t error_flags;
  uint16_t queue_len;
  int noise_floor;
  uint32_t tx_air_secs;
  uint32_t rx_air_secs;
  uint32_t recv_errors;
  float radio_freq;
  float radio_bw;
  uint8_t radio_sf;
  uint8_t radio_cr;
};

class MQTTUplink {
public:
  explicit MQTTUplink(mesh::RTCClock& rtc, mesh::LocalIdentity& identity);

  void begin(FILESYSTEM* fs);
  void end();
  void loop(const MQTTStatusSnapshot& snapshot);
  void publishPacket(const mesh::Packet& packet, bool is_tx, int rssi, float snr, int score = -1,
                     int duration = -1);

  bool isRunning() const { return _running; }
  bool isActive() const;

  void formatStatusReply(char* reply, size_t reply_size) const;
  // Observer CLI: handles the wifi.* / mqtt.* commands, returns false (reply
  // untouched) when the command is not one of ours.
  bool handleCliCommand(WifiLink& wifi, const char* command, char* reply);

  bool setEndpointEnabled(uint8_t bit, bool enabled);
  bool isEndpointEnabled(uint8_t bit) const;
  bool setPacketsEnabled(bool enabled);
  bool isPacketsEnabled() const { return _prefs.packets_enabled != 0; }
  bool setRawEnabled(bool enabled);
  bool isRawEnabled() const { return _prefs.raw_enabled != 0; }
  bool setStatusEnabled(bool enabled);
  bool isStatusEnabled() const { return _prefs.status_enabled != 0; }
  bool setTxEnabled(bool enabled);
  bool isTxEnabled() const { return _prefs.tx_mode != 0; }
  bool setTxMode(uint8_t mode);  // 0 = off, 1 = on, 2 = own adverts only
  uint8_t getTxMode() const { return _prefs.tx_mode; }
  // Drop all broker connections and rejoin them from scratch (CLI `mqtt recover`
  // pairs this with WifiLink::reconnectWifi()).
  void requestReconnect();
  bool setIata(const char* iata);
  const char* getIata() const { return _prefs.iata; }
  const char* getClientVersion() const;
  void setNodeNameSource(const char* node_name) { _node_name = node_name; }
  void setFirmwareVersion(const char* firmware_version) { _firmware_version = firmware_version; }
  bool setOwnerPublicKey(const char* owner_public_key);
  const char* getOwnerPublicKey() const { return _prefs.owner_public_key; }
  bool setOwnerEmail(const char* owner_email);
  const char* getOwnerEmail() const { return _prefs.owner_email; }
  bool sendStatusNow();
  bool isAnyBrokerConnected() const;
  const char* getAggregateBrokerState() const;
  void setNetworkStateProvider(NetworkStateProvider* network) { _network = network; }
  MQTTPrefs& getPrefs() { return _prefs; }
  bool savePrefs();

private:
#if defined(ESP_PLATFORM)
  struct BrokerSpec {
    const char* key;
    const char* label;
    const char* host;
    const char* uri;
    uint8_t bit;
    bool plain_tcp;
    const char* username;
    const char* password;
  };

  struct BrokerState {
    const BrokerSpec* spec;
    esp_mqtt_client_handle_t client;
    bool connected;
    bool connect_announced;
    bool reconnect_pending;
    unsigned long last_connect_attempt;
    unsigned long next_connect_attempt;
    unsigned long connected_since_ms;
    time_t token_expires_at;
    uint8_t reconnect_failures;
    char username[70];
    char* token;
    char client_id[48];
    char status_topic[128];
    char offline_payload[512];
    // diagnostics written from the (static) MQTT event handler
    uint8_t refused_count;        // consecutive broker-side CONNACK refusals
    unsigned long last_err_ms;
    uint8_t last_err_type;        // esp_mqtt_error_type_t
    uint8_t last_err_conn_rc;     // esp_mqtt_connect_return_code_t
    int last_err_sock;            // transport sock_errno
    uint32_t last_err_tls_esp;
    uint32_t last_err_tls_stack;
    uint32_t last_err_cert;
  };
#endif

  FILESYSTEM* _fs;
  mesh::RTCClock* _rtc;
  mesh::LocalIdentity* _identity;
  MQTTPrefs _prefs;
  bool _running;
  unsigned long _last_status_publish;
  MQTTStatusSnapshot _last_status;
  char _device_id[65];
  const char* _node_name;
  const char* _firmware_version;
  NetworkStateProvider* _network;

#if defined(ESP_PLATFORM)
  static constexpr uint8_t kMeshcoretelBit = 0x01;
  static constexpr uint8_t kLetsmeshEuBit = 0x02;
  static constexpr uint8_t kLetsmeshUsBit = 0x04;
  static constexpr uint8_t kMaxEnabledBrokers = 2;
  static const BrokerSpec kBrokerSpecs[3];
  static bool isUnsetIataValue(const char* iata);

  // Bounded best-effort queue for packets captured while no broker is
  // connected (see drainQueue/flushQueue). Payloads are scratch-allocated
  // (PSRAM when available) and owned by the queue.
  struct QueuedEvent {
    char* payload;
    uint8_t attempts;
  };
  static constexpr uint8_t kQueueCapacity = 8;
  static constexpr uint8_t kQueueMaxAttempts = 3;
  static constexpr unsigned long kQueueFlushMillis = 60000;
  QueuedEvent _queue[kQueueCapacity];
  uint8_t _queue_len;
  unsigned long _event_retry_at;
  unsigned long _no_broker_since;  // 0 = at least one broker connected
  uint32_t _stat_rx;
  uint32_t _stat_pub;
  uint32_t _stat_drop;

  BrokerState _brokers[3];

  static void handleMqttEvent(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
  bool hasEnabledBroker() const;
  static uint8_t normalizeEnabledMask(uint8_t mask);
  void formatTopic(char* dst, size_t dst_size, const char* leaf) const;
  void refreshIdentityStrings();
  void refreshBrokerIdentity(BrokerState& broker);
  void refreshBrokerState(BrokerState& broker);
  void ensureBroker(BrokerState& broker, bool allow_new_connect);
  void destroyBroker(BrokerState& broker, bool reset_retry_state = true);
  bool refreshToken(BrokerState& broker);
  void publishStatus(bool online);
  void publishOnlineStatus(BrokerState& broker);
  bool queuePublish(BrokerState& broker, const char* topic, const char* payload, bool retain);
  bool publishToConnected(const char* topic, const char* payload, bool retain);
  void drainQueue(unsigned long now_ms);
  void flushQueue();
  uint8_t queueDepth() const { return _queue_len; }
  int buildStatusJson(char* buffer, size_t buffer_size, bool online) const;
  int buildPacketJson(char* buffer, size_t buffer_size, const mesh::Packet& packet, bool is_tx, int rssi, float snr,
                      int score, int duration) const;
  int buildRawJson(char* buffer, size_t buffer_size, const mesh::Packet& packet, bool is_tx, int rssi, float snr) const;
  static void escapeJsonString(const char* input, char* output, size_t output_size);
  static void makeSafeToken(const char* input, char* output, size_t output_size);
  static void bytesToHexUpper(const uint8_t* src, size_t len, char* dst, size_t dst_size);
  static void formatIsoTimestamp(time_t ts, char* dst, size_t dst_size);
#endif
};

#endif
