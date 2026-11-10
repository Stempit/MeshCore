#!/usr/bin/env bash
#
# flash-update.sh — in-place firmware update that PRESERVES node settings.
#
# Flashes the app image into the first app partition and erases only the
# otadata region (which stores nothing but the "which slot booted last"
# pointer). SPIFFS is never touched, so the node keeps its identity,
# repeater settings and observer settings (wifi / iata / broker switches).
#
# This is what makes observer builds drop-in updatable from stock repeater
# firmware — as long as the partition layout matches (true for every board
# whose env doesn't override board_build.partitions; the min_spiffs.csv
# boards T-Beam 1W and Generic E22 are the exception — use --erase-all
# there when converting from stock, and re-run setup afterwards).
#
# Usage:
#   scripts/flash-update.sh -e <env> -p <port> [--chip esp32|esp32s3|esp32c3] [--erase-all]
#
# Examples:
#   scripts/flash-update.sh -e Heltec_v4_observer_meshcoretel -p /dev/ttyUSB0
#   scripts/flash-update.sh -e Tbeam_SX1262_observer_meshcoretel -p /dev/ttyACM0 --erase-all

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

ENV_NAME=""
PORT=""
CHIP="esp32s3"
ERASE_ALL=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -e) ENV_NAME="$2"; shift 2 ;;
    -p) PORT="$2"; shift 2 ;;
    --chip) CHIP="$2"; shift 2 ;;
    --erase-all) ERASE_ALL=1; shift ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

[[ -n "$ENV_NAME" ]] || { echo "ERROR: -e <env> required (see: ./scripts/build-observer-meshcoretel.sh --list)" >&2; exit 1; }
[[ -n "$PORT" ]] || { echo "ERROR: -p <port> required (e.g. /dev/ttyUSB0, /dev/cu.usbmodem*, COM3)" >&2; exit 1; }
command -v esptool.py >/dev/null 2>&1 || command -v esptool >/dev/null 2>&1 \
  || { echo "ERROR: esptool not found (pip install esptool)" >&2; exit 1; }
ESPTOOL="$(command -v esptool.py || command -v esptool)"

BUILD_DIR=".pio/build/$ENV_NAME"
[[ -f "$BUILD_DIR/firmware.bin" ]] || { echo "ERROR: $BUILD_DIR/firmware.bin not found — build first: pio run -e $ENV_NAME" >&2; exit 1; }
[[ -f "$BUILD_DIR/partitions.bin" ]] || { echo "ERROR: $BUILD_DIR/partitions.bin not found" >&2; exit 1; }

if [[ "$ERASE_ALL" == "1" ]]; then
  [[ -f "$BUILD_DIR/firmware-merged.bin" ]] || { echo "ERROR: firmware-merged.bin missing (run: pio run -e $ENV_NAME -t mergebin)" >&2; exit 1; }
  echo ">> FULL WIPE on $PORT ($ENV_NAME) — all settings will be lost!"
  "$ESPTOOL" --chip "$CHIP" --port "$PORT" --baud 921600 erase_flash
  "$ESPTOOL" --chip "$CHIP" --port "$PORT" --baud 921600 write_flash 0x0 "$BUILD_DIR/firmware-merged.bin"
  echo ">> Done. Node starts fresh — run the setup (wifi/iata) again."
  exit 0
fi

# Extract partition offsets from the built table (32-byte entries):
#   magic(2) type(1) subtype(1) reserved(1) offset(4,LE) size(4,LE) name(16 @byte 12)
read -r APP_OFFSET OTA_OFFSET OTA_SIZE <<EOF
$(python3 - "$BUILD_DIR/partitions.bin" <<'PY'
import struct, sys

data = open(sys.argv[1], "rb").read()
app_off = ota_off = ota_size = None
for i in range(0, len(data) - 31, 32):
    e = data[i:i + 32]
    if e[0] != 0xAA or e[1] != 0x50:
        continue
    etype, name = e[2], e[12:28].split(b"\x00")[0].decode()
    offset, size = struct.unpack("<II", e[4:12])
    if etype == 0x00 and app_off is None:          # first app partition
        app_off = offset
    if etype == 0x01 and name == "otadata":
        ota_off, ota_size = offset, size
if app_off is None or ota_off is None:
    sys.exit("non-OTA partition table — use --erase-all")
print(f"0x{app_off:X} 0x{ota_off:X} 0x{ota_size:X}")
PY
)

echo ">> In-place update on $PORT ($ENV_NAME) — settings are PRESERVED"
echo ">>   app   @ $APP_OFFSET  <- firmware.bin"
echo ">>   otadata @ $OTA_OFFSET (${OTA_SIZE}) erased (slot selection reset)"
"$ESPTOOL" --chip "$CHIP" --port "$PORT" --baud 921600 write_flash "$APP_OFFSET" "$BUILD_DIR/firmware.bin"
"$ESPTOOL" --chip "$CHIP" --port "$PORT" --baud 921600 erase_region "$OTA_OFFSET" "$OTA_SIZE"

echo ">> Done. Identity and settings kept (wifi/iata unchanged if previously set)."
echo ">> Verify after boot: boot screen version + get mqtt.client_version."
