# meshcoretel observer uplink (`WITH_MQTT_UPLINK`)

Optional repeater-firmware module: publishes the packets a repeater hears to
MQTT so mesh analyzers and maps can ingest them. Ported from
VBart/MeshCoreTel-firmware's `src/helpers/mqtt/` (production-proven), adapted
to official upstream APIs. ESP32 only (needs WiFi); everything compiles to
empty without `WITH_MQTT_UPLINK`.

## Brokers

| Key | Endpoint | Auth | Default |
|---|---|---|---|
| `meshcoretel` | `mqtt://meshcoretel.ru:1883` | user/pass `meshcore`/`meshcore` | **on** |
| `letsmesh-eu` | `wss://mqtt-eu-v1.letsmesh.net:443/mqtt` | Ed25519 JWT (`v1_<pubkey>`), TLS pin | off |
| `letsmesh-us` | `wss://mqtt-us-v1.letsmesh.net:443/mqtt` | same | off |

Topics: `meshcore/<IATA>/<pubkey-hex>/packets`, `/status` (retained + LWT
offline), optional `/raw`. Publishing is gated on a sane NTP-synced clock and
`set mqtt.iata` ≠ `UNSET`. A bounded 8-event RAM queue smooths short broker
outages (flushed after 60 s disconnected; oldest replaced when full).

## Build

Env definitions live in the repo-root `observer_meshcoretel.ini`
(`*_observer_meshcoretel` environments). Build with
`scripts/build-observer_meshcoretel.sh` or
`pio run -e <board>_observer_meshcoretel`.

## Configure (serial or mesh remote-admin CLI)

```
set wifi.ssid <ssid>          set wifi.pwd <pass>
set mqtt.iata KVX             # topic address, gates publishing
get mqtt.status               # wifi/ntp/broker states + pkt/pub/drop counters
mqtt recover                  # rebuild WiFi + broker connections
set mqtt.tx advert            # own adverts only (off|on|advert)
set mqtt.raw on               # optional raw-frame feed (off by default)
```

## Security posture (read before deploying)

- **WiFi password and broker credentials are stored unencrypted** in
  `/mqtt_prefs` on the device filesystem. Anyone with filesystem access can
  read them; protect physical access and CLI credentials.
- **meshcoretel runs over plain TCP with shared credentials**
  (`meshcore`/`meshcore`). Telemetry is readable and injectable by anyone
  on-path; the topic identifies your node by its full public key. This is the
  broker's published design, not a firmware choice.
- **The observer captures third parties' mesh traffic.** Enabling the raw
  feed (`set mqtt.raw on`) uplinks full undecrypted frames heard on the
  shared channel — treat that as an operator-consent decision.
- **CLI is the trust boundary**: anyone who can run `set` commands can change
  where your telemetry goes. `get wifi.ssid` shows the SSID but no password
  command exists.
- LetsMesh JWTs are signed with the node's Ed25519 identity; `owner` must be
  64 hex chars, `email` is charset-validated (alnum plus `@._-+`, <64 chars).

## Timing

SNTP (RU pool servers by design: `0.ru.pool.ntp.org`, `ntp.ix.ru`) starts when
WiFi connects; UTC timestamps gate all publishing until the clock is sane.
