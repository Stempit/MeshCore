#!/usr/bin/env bash
#
# build-observer-meshcoretel.sh — build the meshcoretel observer firmware
# (repeater + WiFi/MQTT uplink to mqtt.meshcoretel.ru) for every supported
# ESP32 board, on top of a chosen upstream MeshCore release.
#
# Usage:
#   scripts/build-observer-meshcoretel.sh [repeater-vX.Y.Z | commit] [--env NAME]
#   scripts/build-observer-meshcoretel.sh --list
#
# Examples:
#   scripts/build-observer-meshcoretel.sh                          # latest upstream repeater-v* tag
#   scripts/build-observer-meshcoretel.sh repeater-v1.18.0         # specific upstream release tag
#   scripts/build-observer-meshcoretel.sh repeater-v1.18.0 --env Heltec_v3_observer_meshcoretel
#
# The mqtt-uplink branch is rebased onto the target release (our diff is one
# additive module + small hooks, so this is almost always clean), every
# *_observer_meshcoretel environment is built, and firmware images are
# collected into out/observer-meshcoretel/<version>/<env>/.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BRANCH="mqtt-uplink"
UPSTREAM_URL="https://github.com/meshcore-dev/MeshCore.git"
OUT_DIR="out/observer-meshcoretel"

ALL_ENVS=(
  Heltec_v2_observer_meshcoretel
  Heltec_v3_observer_meshcoretel
  Heltec_v4_observer_meshcoretel
  Heltec_v4_tft_observer_meshcoretel
  Heltec_Wireless_Paper_observer_meshcoretel
  Heltec_E213_observer_meshcoretel
  Heltec_E290_observer_meshcoretel
  Heltec_T190_observer_meshcoretel
  LilyGo_TDeck_observer_meshcoretel
  LilyGo_TLora_V2_1_1_6_observer_meshcoretel
  Tbeam_SX1262_observer_meshcoretel
  LilyGo_TBeam_1W_observer_meshcoretel
  LilyGo_T3S3_sx1262_observer_meshcoretel
  Xiao_S3_observer_meshcoretel
  Xiao_S3_WIO_observer_meshcoretel
  Meshadventurer_sx1262_observer_meshcoretel
  Generic_E22_sx1262_observer_meshcoretel
  Heltec_v2_observer_kvx
  Heltec_v3_observer_kvx
  Heltec_v4_observer_kvx
  Heltec_v4_tft_observer_kvx
  Heltec_Wireless_Paper_observer_kvx
  Heltec_E213_observer_kvx
  Heltec_E290_observer_kvx
  Heltec_T190_observer_kvx
  LilyGo_TDeck_observer_kvx
  LilyGo_TLora_V2_1_1_6_observer_kvx
  Tbeam_SX1262_observer_kvx
  LilyGo_TBeam_1W_observer_kvx
  LilyGo_T3S3_sx1262_observer_kvx
  Xiao_S3_observer_kvx
  Xiao_S3_WIO_observer_kvx
  Meshadventurer_sx1262_observer_kvx
  Generic_E22_sx1262_observer_kvx
  Heltec_v4_kvx
  Heltec_v3_kvx
  ProMicro_kvx
  Heltec_t114_kvx
  Heltec_t096_kvx
  LilyGo_T-Echo_kvx
)

TARGET=""
ONLY_ENV=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --env)
      ONLY_ENV="$2"
      shift 2
      ;;
    --env=*)
      ONLY_ENV="${1#--env=}"
      shift
      ;;
    --list)
      echo "Environments:"
      for e in "${ALL_ENVS[@]}"; do echo "  $e"; done
      exit 0
      ;;
    *)
      TARGET="$1"
      shift
      ;;
  esac
done

if ! command -v pio >/dev/null 2>&1; then
  echo "ERROR: PlatformIO not found on PATH (install: pip install -U platformio)" >&2
  exit 1
fi

# --- resolve target: explicit arg, else latest upstream repeater-v* tag -------
git remote get-url upstream >/dev/null 2>&1 || git remote add upstream "$UPSTREAM_URL"
echo ">> git fetch upstream (tags)"
git fetch upstream --tags --quiet

if [[ -z "$TARGET" ]]; then
  TARGET="$(git tag --list 'repeater-v*' --merged upstream/main --sort=-v:refname --format='%(refname:short)' | head -1)"
  if [[ -z "$TARGET" ]]; then
    echo "ERROR: no repeater-v* tag found on upstream" >&2
    exit 1
  fi
fi
echo ">> target release: $TARGET"

# --- rebase our additive branch onto the target -------------------------------
CURRENT="$(git branch --show-current)"
if [[ "$CURRENT" != "$BRANCH" ]]; then
  echo ">> switching to $BRANCH (was $CURRENT)"
  git checkout "$BRANCH" --quiet
fi

TARGET_SHA="$(git rev-parse --verify --quiet "$TARGET^{commit}")"
[[ -n "$TARGET_SHA" ]] || { echo "ERROR: unknown target '$TARGET'" >&2; exit 1; }

if git merge-base --is-ancestor "$TARGET_SHA" HEAD; then
  echo ">> $TARGET already contained in HEAD, skipping rebase"
else
  echo ">> rebasing $BRANCH onto $TARGET ..."
  if ! git rebase "$TARGET_SHA"; then
    echo "" >&2
    echo "REBASE CONFLICT — resolve by hand, then:" >&2
    echo "  git rebase --continue   (or: git rebase --abort)" >&2
    echo "and re-run this script." >&2
    exit 2
  fi
fi

if [[ "$TARGET" == repeater-v* ]]; then
  VERSION="${TARGET#repeater-v}"
else
  VERSION="$(git rev-parse --short "$TARGET_SHA")"
fi
DEST="$OUT_DIR/$VERSION"
mkdir -p "$DEST"

# --- build ----------------------------------------------------------------------

# Refresh the build stamp: __DATE__/__TIME__ live in ObserverMesh.cpp; touching
# this one file makes every env recompile just that TU with the current time.
touch examples/simple_repeater/ObserverMesh.cpp

ENVS=("${ALL_ENVS[@]}")
if [[ -n "$ONLY_ENV" ]]; then
  ENVS=("$ONLY_ENV")
fi

# validate env names exist in the merged project config
FAILED=0
for ENV_NAME in "${ENVS[@]}"; do
  if ! grep -q "\[env:${ENV_NAME}\]" observer_meshcoretel.ini observer_meshcoretel_kvx.ini repeater_kvx.ini variants/*/platformio.ini; then
    echo "ERROR: env '$ENV_NAME' not found in the project ini files" >&2
    FAILED=1
  fi
done
if [[ "$FAILED" != "0" ]]; then
  echo "FINISHED WITH FAILURES — fix the env names and re-run" >&2
  exit 1
fi

# Pre-warm dependency resolution sequentially: when parallel pio processes
# resolve libdeps on a cold shared cache, each clones git deps (e.g. the
# pinned RadioLib) itself. Resolving one env at a time populates the shared
# mirror once, so parallel builds never race the network. Only cold envs are
# resolved; each call is time-bounded so a stalled fetch can't block the run.
for ENV_NAME in "${ENVS[@]}"; do
  [[ -d ".pio/libdeps/$ENV_NAME" ]] && continue
  echo ">> pre-warming deps: $ENV_NAME"
  perl -e 'alarm shift; exec @ARGV' 120 pio pkg install -e "$ENV_NAME" -s >/dev/null 2>&1 \
    || echo "WARN: dependency pre-warm skipped for $ENV_NAME (timeout/failure — its build may fetch deps itself)" >&2
done

# Parallel build: BUILD_JOBS envs at a time (default 4). Envs build in separate
# .pio/build/<env> dirs, so concurrent pio processes don't collide; per-env
# output is kept in a log file. BUILD_JOBS=1 restores sequential behavior.
JOBS="${BUILD_JOBS:-4}"

# Build cache (SCons CacheDir via PlatformIO's build_cache_dir option): object
# files are stored/restored by hash, so a rebuild after rebasing onto a new
# upstream release only compiles genuinely changed sources. Enabled in the
# gitignored platformio.local.ini (loaded by upstream extra_configs) so no
# upstream file changes; skipped if the user configured their own.
LOCAL_INI="platformio.local.ini"
if ! grep -q "build_cache_dir" "$LOCAL_INI" 2>/dev/null; then
  echo ">> enabling build cache in $LOCAL_INI"
  printf '\n[platformio]\nbuild_cache_dir = %s/.pio/build_cache\n' "$REPO_ROOT" >> "$LOCAL_INI"
fi

# Split the CPU budget between concurrent envs (each pio compiles with -j jobs)
NCPU="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
PIO_JOBS=$(( NCPU / JOBS )); [[ "$PIO_JOBS" -lt 1 ]] && PIO_JOBS=1

# Release asset identity, upstream-style: <EnvName>-v<version>-<shortSHA>-...
SHA="$(git rev-parse --short=7 HEAD)"
VVER="v${VERSION}"

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/observer-build-logs.XXXXXX")"
FAIL_LIST="$LOG_DIR/failures.txt"
: > "$FAIL_LIST"

build_one() {
  local ENV_NAME="$1"
  local LOG="$LOG_DIR/$ENV_NAME.log"
  echo ">> building $ENV_NAME"
  if ! pio run -e "$ENV_NAME" -j "$PIO_JOBS" >"$LOG" 2>&1; then
    echo "$ENV_NAME" >> "$FAIL_LIST"
    echo ">> BUILD FAILED: $ENV_NAME (log: $LOG)" >&2
    return 1
  fi
  local BUILD_DIR=".pio/build/$ENV_NAME" ENV_DEST="$DEST/$ENV_NAME" f
  mkdir -p "$ENV_DEST"
  # Platform-specific post-processing (both are upstream custom targets):
  # ESP32 -> merged flashable image; nRF52 -> uf2 (drag-and-drop flashing).
  if [[ -f "$BUILD_DIR/bootloader.bin" ]]; then
    pio run -e "$ENV_NAME" -t mergebin >>"$LOG" 2>&1 || echo "WARN: mergebin failed for $ENV_NAME" >&2
  else
    pio run -e "$ENV_NAME" -t create_uf2 >>"$LOG" 2>&1 || echo "WARN: create_uf2 failed for $ENV_NAME" >&2
  fi
  # Release asset names follow the upstream convention
  # (<EnvName>-v<version>-<shortSHA>-merged.bin|.bin|.uf2|.zip); only
  # flashable images ship — no ELF/hex/bootloader/partitions.
  if [[ -f "$BUILD_DIR/firmware-merged.bin" ]]; then
    cp "$BUILD_DIR/firmware-merged.bin" "$ENV_DEST/${ENV_NAME}-${VVER}-${SHA}-merged.bin"
  fi
  if [[ -f "$BUILD_DIR/firmware.bin" ]]; then
    cp "$BUILD_DIR/firmware.bin" "$ENV_DEST/${ENV_NAME}-${VVER}-${SHA}.bin"
  fi
  if [[ -f "$BUILD_DIR/firmware.uf2" ]]; then
    cp "$BUILD_DIR/firmware.uf2" "$ENV_DEST/${ENV_NAME}-${VVER}-${SHA}.uf2"
  fi
  if [[ -f "$BUILD_DIR/firmware.zip" ]]; then
    cp "$BUILD_DIR/firmware.zip" "$ENV_DEST/${ENV_NAME}-${VVER}-${SHA}.zip"
  fi
  echo ">> done: $ENV_NAME"
}

for ENV_NAME in "${ENVS[@]}"; do
  while [[ "$(jobs -rp | wc -l | tr -d ' ')" -ge "$JOBS" ]]; do
    sleep 2
  done
  build_one "$ENV_NAME" &
done
wait

echo ""
if [[ -s "$FAIL_LIST" ]]; then
  echo "FINISHED WITH FAILURES:" >&2
  cat "$FAIL_LIST" >&2
  echo "per-env logs: $LOG_DIR" >&2
  exit 1
fi
echo ">> all builds OK — artifacts in $DEST/ (per-env logs: $LOG_DIR)"
