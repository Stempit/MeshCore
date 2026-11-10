# Adding a new device to the build

The observer firmware runs on **ESP32-family boards with WiFi**
(ESP32 / ESP32-S3 / ESP32-C3). Boards without WiFi (nRF52 like RAK 4631 or
ThinkNode M1, STM32, RP2040) cannot run it — for those, use a host-side
bridge (e.g. the `meshcore-go` PacketPublisher over serial).

## Prerequisites

- The board already has a working **repeater** environment in upstream
  MeshCore: check `variants/*/platformio.ini` or run
  `pio run --list | grep -i repeater`. If upstream doesn't support the board
  as a repeater yet, that's an upstream contribution first — out of scope here.

## Steps

### 1. Add an environment in `observer_meshcoretel.ini`

Append a block at the end of the root `observer_meshcoretel.ini`, deriving
from the board's existing repeater environment:

```ini
[env:<Board>_observer_meshcoretel]
extends = env:<Board>_repeater
build_flags = ${env:<Board>_repeater.build_flags} -D WITH_MQTT_UPLINK=1
build_src_filter = ${env:<Board>_repeater.build_src_filter} +<helpers/mqtt/*.cpp>
```

That's all: the block turns on the observer code and adds `src/helpers/mqtt/`
to the build. Nothing else in the repo needs to change — this file is merged
into PlatformIO's config via `extra_configs` in the root `platformio.ini`.

### 2. Build it

```sh
pio run -e <Board>_observer_meshcoretel
```

Check the reported **RAM** and **Flash** percentages.

### 3. "Flash is full"? (4 MB classic-ESP32 boards)

WiFi + MQTT adds ~170 KB to the app. Boards whose stock partition table only
gives a 1.25 MB app slot (typical for classic ESP32 / 4 MB boards) will
overflow. Fix inside your env block:

```ini
; stock 1.25MB app partition cannot fit WiFi+MQTT uplink; min_spiffs gives a
; 1.92MB app slot and keeps SPIFFS for identity/prefs (trade-off: no OTA slot)
board_build.partitions = min_spiffs.csv
```

Existing examples: `LilyGo_TBeam_1W_observer_meshcoretel` and
`Generic_E22_sx1262_observer_meshcoretel` in
`observer_meshcoretel.ini`. Note the trade-off: that board loses its OTA
update slot, and converting a node between partition layouts resets its
stored settings (see [build-update.md](build-update.md)).

### 4. Register the board in the release script

Add the new env name to `ALL_ENVS` in `scripts/build-observer_meshcoretel.sh`
so release builds include it. For the KVX families, also append an env to
`observer_meshcoretel_kvx.ini` (extends your `_observer_meshcoretel` env) or
`repeater_kvx.ini` (extends the upstream repeater env), each adding only:

```ini
build_flags = ${env:<parent>.build_flags} -D WITH_KVX_DEFAULTS=1
```

### 5. Verify

```sh
./scripts/build-observer_meshcoretel.sh --env <Board>_observer_meshcoretel
```

Then flash and set the node up as usual ([setup.md](setup.md)) — the real test
is `get mqtt.status` showing `meshcoretel:up` and packets appearing on
meshcoretel.ru under your IATA.
