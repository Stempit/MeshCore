#include "WifiLink.h"

#ifdef WITH_MQTT_UPLINK

#include <helpers/TxtDataHelpers.h>
#include <string.h>
#include <time.h>

#if defined(ESP_PLATFORM)
  #include <WiFi.h>
  #include <esp_sntp.h>
#endif

namespace {

#if defined(ESP_PLATFORM)
constexpr unsigned long kWifiRetryMillis = 15000;
constexpr unsigned long kWifiConnectTimeoutMillis = 45000;
constexpr time_t kMinSaneEpoch = 1767225600;  // 2026-01-01T00:00:00Z
constexpr time_t kMaxOutOfSync = 86400;       // 24h

int getWifiQualityPercent(int rssi_dbm) {
  if (rssi_dbm <= -100) {
    return 0;
  }
  if (rssi_dbm >= -50) {
    return 100;
  }
  return 2 * (rssi_dbm + 100);
}

const char* getWifiQualityLabel(int rssi_dbm) {
  if (rssi_dbm >= -60) {
    return "excellent";
  }
  if (rssi_dbm >= -67) {
    return "good";
  }
  if (rssi_dbm >= -75) {
    return "fair";
  }
  return "poor";
}
#endif

}  // namespace

WifiLink::WifiLink()
    : _prefs(nullptr), _wifi_started(false), _sntp_started(false), _have_time_sync(false), _last_wifi_attempt(0),
      _last_time_sync(0) {
}

void WifiLink::loop(bool network_required) {
#if defined(ESP_PLATFORM)
  ensureWifi(network_required);
  updateTimeSync();
#else
  (void)network_required;
#endif
}

bool WifiLink::setWifiPowerSave(const char* mode) {
#if defined(ESP_PLATFORM)
  if (mode == nullptr || _prefs == nullptr) {
    return false;
  }

  uint8_t next_mode;
  if (strcmp(mode, "none") == 0) {
    next_mode = 0;
  } else if (strcmp(mode, "min") == 0) {
    next_mode = 1;
  } else if (strcmp(mode, "max") == 0) {
    next_mode = 2;
  } else {
    return false;
  }

  // Mutates the shared MQTTPrefs in memory; persistence is the caller's job
  // (MQTTUplink::handleCliCommand calls savePrefs()).
  _prefs->wifi_powersave = next_mode;
  if (_wifi_started) {
    WiFi.setSleep(toEspPowerSave(next_mode));
  }
  return true;
#else
  (void)mode;
  return false;
#endif
}

const char* WifiLink::getWifiPowerSave() const {
#if defined(ESP_PLATFORM)
  if (_prefs == nullptr) {
    return "none";
  }
  return getPowerSaveLabel(_prefs->wifi_powersave);
#else
  return "unsupported";
#endif
}

bool WifiLink::setWifiSSID(const char* ssid) {
#if defined(ESP_PLATFORM)
  if (ssid == nullptr || _prefs == nullptr) {
    return false;
  }
  StrHelper::strncpy(_prefs->wifi_ssid, ssid, sizeof(_prefs->wifi_ssid));
  reconnectWifi();
  return true;
#else
  (void)ssid;
  return false;
#endif
}

bool WifiLink::setWifiPassword(const char* pwd) {
#if defined(ESP_PLATFORM)
  if (pwd == nullptr || _prefs == nullptr) {
    return false;
  }
  StrHelper::strncpy(_prefs->wifi_pwd, pwd, sizeof(_prefs->wifi_pwd));
  reconnectWifi();
  return true;
#else
  (void)pwd;
  return false;
#endif
}

const char* WifiLink::getWifiSSID() const {
#if defined(ESP_PLATFORM)
  return (_prefs != nullptr) ? _prefs->wifi_ssid : "";
#else
  return "";
#endif
}

void WifiLink::formatWifiStatusReply(char* reply, size_t reply_size) const {
#if defined(ESP_PLATFORM)
  const char* ssid = (_prefs != nullptr && _prefs->wifi_ssid[0]) ? _prefs->wifi_ssid : "";
  const char* status = "disconnected";
  const char* state = "disconnected";
  wl_status_t wifi_status = WiFi.status();
  if (ssid[0] == 0) {
    status = "unconfigured";
    state = "unconfigured";
  } else if (wifi_status == WL_CONNECTED) {
    status = "connected";
    state = "connected";
  } else if (_wifi_started) {
    status = "connecting";
  }

  switch (wifi_status) {
    case WL_IDLE_STATUS:
      state = "idle";
      break;
    case WL_NO_SSID_AVAIL:
      state = "no_ssid";
      break;
    case WL_SCAN_COMPLETED:
      state = "scan_completed";
      break;
    case WL_CONNECTED:
      state = "connected";
      break;
    case WL_CONNECT_FAILED:
      state = "connect_failed";
      break;
    case WL_CONNECTION_LOST:
      state = "connection_lost";
      break;
    case WL_DISCONNECTED:
      state = "disconnected";
      break;
    default:
      state = "unknown";
      break;
  }

  if (wifi_status == WL_CONNECTED) {
    const int rssi_dbm = WiFi.RSSI();
    snprintf(reply, reply_size, "> ssid:%s status:%s code:%d state:%s ip:%s rssi:%d quality:%d%% signal:%s", ssid,
             status, static_cast<int>(wifi_status), state, WiFi.localIP().toString().c_str(), rssi_dbm,
             getWifiQualityPercent(rssi_dbm), getWifiQualityLabel(rssi_dbm));
  } else {
    snprintf(reply, reply_size, "> ssid:%s status:%s code:%d state:%s", ssid[0] ? ssid : "-", status,
             static_cast<int>(wifi_status), state);
  }
#else
  (void)reply_size;
  snprintf(reply, reply_size, "> wifi:unsupported");
#endif
}

void WifiLink::reconnectWifi() {
#if defined(ESP_PLATFORM)
  if (_wifi_started) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  }
#endif
  _wifi_started = false;
  _sntp_started = false;
  _have_time_sync = false;
  _last_wifi_attempt = 0;
}

bool WifiLink::isWifiConnected() const {
#if defined(ESP_PLATFORM)
  return _wifi_started && WiFi.status() == WL_CONNECTED;
#else
  return false;
#endif
}

bool WifiLink::hasTimeSync() const {
#if defined(ESP_PLATFORM)
  if (_have_time_sync) return true;
  time_t now = time(nullptr);
  return now >= kMinSaneEpoch && now < (_last_time_sync + kMaxOutOfSync);
#else
  return false;
#endif
}

#if defined(ESP_PLATFORM)

uint8_t WifiLink::toEspPowerSave(uint8_t mode) {
  switch (mode) {
    case 1:
      return WIFI_PS_MIN_MODEM;
    case 2:
      return WIFI_PS_MAX_MODEM;
    default:
      return WIFI_PS_NONE;
  }
}

const char* WifiLink::getPowerSaveLabel(uint8_t mode) {
  switch (mode) {
    case 1:
      return "min";
    case 2:
      return "max";
    default:
      return "none";
  }
}

void WifiLink::ensureWifi(bool network_required) {
  if (!network_required) {
    if (_wifi_started) {
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      _wifi_started = false;
      _sntp_started = false;
      _have_time_sync = false;
    }
    return;
  }

  const char* ssid = (_prefs != nullptr) ? _prefs->wifi_ssid : "";
  const char* pwd = (_prefs != nullptr) ? _prefs->wifi_pwd : "";
  if (ssid[0] == 0) {
    reconnectWifi();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now_ms = millis();
  wl_status_t status = WiFi.status();
  if (_wifi_started) {
    if (_last_wifi_attempt != 0 && status == WL_IDLE_STATUS && now_ms - _last_wifi_attempt < kWifiConnectTimeoutMillis) {
      return;
    }
    if (now_ms - _last_wifi_attempt < kWifiRetryMillis) {
      return;
    }
  }

  if (!_wifi_started) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(toEspPowerSave(_prefs != nullptr ? _prefs->wifi_powersave : 0));
    sntp_servermode_dhcp(1);
    _wifi_started = true;
  }

  _last_wifi_attempt = now_ms;
  WiFi.begin(ssid, pwd);
}

void WifiLink::updateTimeSync() {
  bool prev_have_time_sync = _have_time_sync;
  if (!_wifi_started || WiFi.status() != WL_CONNECTED) {
    _have_time_sync = false;
    return;
  }

  if (!_sntp_started) {
    setenv("TZ", "UTC0", 1);
    tzset();

    const ip_addr_t* dhcp_srv = sntp_getserver(0);
    bool have_dhcp_ntp = dhcp_srv && !ip_addr_isany(dhcp_srv);

    if (have_dhcp_ntp) {
      sntp_setservername(1, "0.ru.pool.ntp.org");
      sntp_setservername(2, "ntp.ix.ru");
    } else {
      sntp_setservername(0, "0.ru.pool.ntp.org");
      sntp_setservername(1, "ntp.ix.ru");
      sntp_setservername(2, "ntp21.vniiftri.ru");
    }

    sntp_init();
    _sntp_started = true;
  }

  sntp_sync_status_t sync_status = sntp_get_sync_status();
  time_t now = time(nullptr);
  bool sane_time = now >= kMinSaneEpoch;
  bool sync_ready = sync_status == SNTP_SYNC_STATUS_COMPLETED || sync_status == SNTP_SYNC_STATUS_IN_PROGRESS;
  _have_time_sync = sane_time && (sync_ready || prev_have_time_sync);

  if (_have_time_sync) _last_time_sync = now;
}

#endif
#endif
