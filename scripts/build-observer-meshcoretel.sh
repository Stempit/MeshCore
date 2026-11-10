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
ENVS=("${ALL_ENVS[@]}")
if [[ -n "$ONLY_ENV" ]]; then
  ENVS=("$ONLY_ENV")
fi

# validate env names exist in the merged project config
FAILED=0
for ENV_NAME in "${ENVS[@]}"; do
  if ! grep -q "\[env:${ENV_NAME}\]" observer_meshcoretel.ini variants/*/platformio.ini; then
    echo "ERROR: env '$ENV_NAME' not found in observer_meshcoretel.ini or variants/" >&2
    FAILED=1
    continue
  fi
  echo ""
  echo "==================================================================="
  echo ">> building $ENV_NAME"
  echo "==================================================================="
  if ! pio run -e "$ENV_NAME"; then
    echo "BUILD FAILED: $ENV_NAME" >&2
    FAILED=1
    continue
  fi
  # upstream merge-bin.py exposes the flashable single image as a custom target
  pio run -e "$ENV_NAME" -t mergebin >/dev/null 2>&1 || echo "WARN: mergebin target failed for $ENV_NAME" >&2
  BUILD_DIR=".pio/build/$ENV_NAME"
  ENV_DEST="$DEST/$ENV_NAME"
  mkdir -p "$ENV_DEST"
  for f in firmware.bin firmware.elf bootloader.bin partitions.bin boot_app0.bin; do
    [[ -f "$BUILD_DIR/$f" ]] && cp "$BUILD_DIR/$f" "$ENV_DEST/"
  done
  # merged image (flashable single bin) produced by upstream merge-bin.py
  MERGED="$(find "$BUILD_DIR" -maxdepth 1 -name '*merged.bin' | head -1)"
  [[ -n "$MERGED" ]] && cp "$MERGED" "$ENV_DEST/"
  echo ">> artifacts: $ENV_DEST"
done

echo ""
if [[ "$FAILED" != "0" ]]; then
  echo "FINISHED WITH FAILURES — see output above" >&2
  exit 1
fi
echo ">> all builds OK — artifacts in $DEST/"
