#!/usr/bin/env bash
#
# make-release.sh — create/update the three GitHub releases for a mod release
# and upload the matching firmware assets from out/dist/.
#
# Releases (one per firmware family, matching the git tags):
#   repeater_v<BASE>_kvx_<MODV>                  <- out/dist/repeater_kvx/
#   observer_v<BASE>_meshcoretel_<MODV>          <- out/dist/observer_meshcoretel/
#   observer_v<BASE>_meshcoretel_kvx_<MODV>      <- out/dist/observer_meshcoretel_kvx/
#
# Usage:
#   scripts/make-release.sh [--dry-run]
#
# Requirements:
#   - GITHUB_TOKEN env var set (GitHub PAT with repo scope)
#   - the three tags already exist (create with: git tag -a <tag> HEAD)
#   - out/dist/ populated (run scripts/build-observer-meshcoretel.sh first)
#
# Idempotent: existing releases are reused, assets with the same file name are
# replaced.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

DRY_RUN=0
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=1

OWNER="Stempit"
REPO="MeshCore"
API="https://api.github.com/repos/$OWNER/$REPO"
UPLOAD="https://uploads.github.com/repos/$OWNER/$REPO"

# --- versions ------------------------------------------------------------------
MODV="$(grep '#define MESHCORETEL_MOD_VERSION' src/helpers/mqtt/ModVersion.h | grep -oE '"v[^"]+"' | tr -d '"')"
[[ -n "$MODV" ]] || { echo "ERROR: cannot parse MESHCORETEL_MOD_VERSION" >&2; exit 1; }

BASE_TAG="$(git tag --list 'repeater-v*' --merged HEAD --sort=-v:refname | head -1)"
[[ -n "$BASE_TAG" ]] || { echo "ERROR: no repeater-v* tag merged into HEAD" >&2; exit 1; }
BASE_VER="${BASE_TAG#repeater-v}"      # e.g. 1.18.0
BASE="v${BASE_VER}"                    # e.g. v1.18.0

echo ">> mod version: $MODV, base release: $BASE_TAG"

# --- release definitions ---------------------------------------------------------
# tag | dist group | title | notes
NOTES_COMMON="## Setup

See [howto/](https://github.com/$OWNER/$REPO/blob/mqtt-uplink/howto/) (English + Russian).
Radio params are comma-separated: \`set radio 868.731,62.5,8,8\`.

## Установка

См. [howto/](https://github.com/$OWNER/$REPO/blob/mqtt-uplink/howto/) (EN + RU).
Радиопараметры задаются через запятую: \`set radio 868.731,62.5,8,8\`."

NOTES_REPEATER_KVX="KVX vanilla repeater firmware — stock repeater + first-boot KVX regional
defaults (868.731/62.5/SF8/CR8, 2-byte path hash, cad on, txdelay 2, rxdelay 5,
tx 22, name \`KRV_repeater\`, regions ru/kvx). **No WiFi/MQTT** — nRF52 boards.

Фирмваре ретранслятора с KVX-дефолтами по умолчанию, без WiFi/MQTT (nRF52).

Flash: drag the \`.uf2\` onto the board's USB drive (double-tap reset first),
or DFU with the \`.zip\`.

$NOTES_COMMON"

NOTES_OBSERVER="meshcoretel **observer** firmware — repeater + WiFi/MQTT telemetry to
[meshcoretel.ru](https://meshcoretel.ru). ESP32 boards only (needs WiFi).

Наблюдатель: ретранслятор + WiFi/MQTT-телеметрия на meshcoretel.ru (только ESP32).

Flash \`-merged.bin\` via the [web flasher](https://flasher.meshcore.dev)
(board first → Custom Firmware) or \`erase_flash\` + esptool.
Setup: \`set wifi.ssid\`, \`set wifi.pwd\`, \`set mqtt.iata KVX\`.

$NOTES_COMMON"

NOTES_OBSERVER_KVX="meshcoretel **observer + KVX** firmware — the observer above, plus
first-boot KVX regional defaults (radio, path hash, cad, delays, name
\`KRV_repeater\`, regions ru/kvx, \`mqtt.iata KVX\`). Only the WiFi
credentials are needed on a fresh node.

Наблюдатель + KVX-дефолты: на свежем узле достаточно задать только WiFi.

Flash \`-merged.bin\` via the [web flasher](https://flasher.meshcore.dev)
(board first → Custom Firmware) or \`erase_flash\` + esptool.

$NOTES_COMMON"

RELEASES=(
  "repeater_v${BASE_VER}_kvx_${MODV}|repeater_kvx|KVX Repeater ${MODV} (base ${BASE})|${NOTES_REPEATER_KVX}"
  "observer_v${BASE_VER}_meshcoretel_${MODV}|observer_meshcoretel|meshcoretel Observer ${MODV} (base ${BASE})|${NOTES_OBSERVER}"
  "observer_v${BASE_VER}_meshcoretel_kvx_${MODV}|observer_meshcoretel_kvx|meshcoretel Observer + KVX ${MODV} (base ${BASE})|${NOTES_OBSERVER_KVX}"
)

# --- sanity checks ----------------------------------------------------------------
[[ -n "${GITHUB_TOKEN:-}" ]] || {
  [[ "$DRY_RUN" == "1" ]] || {
    echo "ERROR: GITHUB_TOKEN not set (GitHub PAT with repo scope:" >&2
    echo "  export GITHUB_TOKEN=github_pat_... )" >&2
    exit 1
  }
}
for entry in "${RELEASES[@]}"; do
  TAG="${entry%%|*}"
  git rev-parse --verify --quiet "refs/tags/$TAG" >/dev/null || {
    echo "ERROR: tag '$TAG' does not exist — create it first (git tag -a $TAG HEAD)" >&2
    exit 1
  }
done
[[ -d "out/dist/repeater_kvx" ]] || {
  echo "ERROR: out/dist/ missing — run scripts/build-observer-meshcoretel.sh first" >&2
  exit 1
}

# --- helpers ----------------------------------------------------------------------
api() { curl -sS -H "Authorization: Bearer $GITHUB_TOKEN" "$@"; }

release_id_for() {
  local TAG="$1"
  api "$API/releases/tags/$TAG" | python3 -c "
import json, sys
d = json.load(sys.stdin)
print(d.get('id', ''))
" 2>/dev/null
}

upload_asset() {
  local RELEASE_ID="$1" FILE="$2"
  local NAME base
  NAME="$(basename "$FILE")"
  # idempotent: drop an existing asset with the same name first
  local EXISTING
  EXISTING=$(api "$API/releases/$RELEASE_ID/assets?per_page=100" | python3 -c "
import json, sys
d = json.load(sys.stdin)
name = '$NAME'
print(next((a['id'] for a in d if a['name'] == name), ''))
" 2>/dev/null)
  if [[ -n "$EXISTING" ]]; then
    api -X DELETE "$API/releases/assets/$EXISTING" >/dev/null
  fi
  api -X POST -H "Authorization: Bearer $GITHUB_TOKEN" \
      -H "Content-Type: application/octet-stream" \
      --data-binary "@$FILE" \
      "$UPLOAD/releases/$RELEASE_ID/assets?name=$NAME" \
      | python3 -c "
import json, sys
d = json.load(sys.stdin)
print('uploaded:', d.get('name', '?'), d.get('state', d.get('message', 'ERROR')))
"
}

# --- main ---------------------------------------------------------------------------
if [[ "$DRY_RUN" == "1" ]]; then
  echo ">> DRY RUN — no API calls"
fi

FAILED=0
for entry in "${RELEASES[@]}"; do
  IFS='|' read -r TAG GROUP TITLE NOTES <<< "$entry"
  DIR="out/dist/$GROUP"
  echo ""
  echo "== $TAG (assets from $DIR) =="
  local_count="$(ls "$DIR" | wc -l | tr -d ' ')"
  [[ "$local_count" -gt 0 ]] || { echo "ERROR: no assets in $DIR" >&2; FAILED=1; continue; }

  if [[ "$DRY_RUN" == "1" ]]; then
    ls "$DIR"
    continue
  fi

  RELEASE_ID="$(release_id_for "$TAG")"
  if [[ -z "$RELEASE_ID" ]]; then
    echo ">> creating release $TAG"
    BODY_JSON="$(python3 -c "
import json
print(json.dumps({'tag_name': '$TAG', 'name': '''$TITLE''', 'body': open('/dev/stdin').read(), 'draft': False, 'prerelease': False}))
" <<< "$NOTES")"
    RELEASE_ID="$(api -X POST -H "Authorization: Bearer $GITHUB_TOKEN" -d "$BODY_JSON" "$API/releases" \
      | python3 -c "
import json, sys
d = json.load(sys.stdin)
print(d.get('id', ''))
")"
  else
    echo ">> release exists (id $RELEASE_ID) — updating body and assets"
    BODY_JSON="$(python3 -c "
import json
print(json.dumps({'tag_name': '$TAG', 'name': '''$TITLE''', 'body': open('/dev/stdin').read()}))
" <<< "$NOTES")"
    api -X PATCH -H "Authorization: Bearer $GITHUB_TOKEN" -d "$BODY_JSON" "$API/releases/$RELEASE_ID" >/dev/null
  fi
  [[ -n "$RELEASE_ID" ]] || { echo "ERROR: could not create/fetch release $TAG" >&2; FAILED=1; continue; }

  for f in "$DIR"/*; do
    upload_asset "$RELEASE_ID" "$f"
  done
done

echo ""
[[ "$FAILED" != "0" ]] && { echo "FINISHED WITH FAILURES" >&2; exit 1; }
echo ">> releases ready: https://github.com/$OWNER/$REPO/releases"
