# Building the observer firmware from scratch

Target audience: a fresh machine that has never built this project. End
result: flashable firmware images in `out/observer_meshcoretel/<version>/`.

## 1. Prerequisites

- **git**
- **Python 3.9+** (PlatformIO runs on it)
- **PlatformIO Core**:

  ```sh
  pip install -U platformio
  # verify:
  pio --version
  ```

- ~2 GB free disk space (ESP32 toolchains + frameworks are downloaded on the
  first build) and internet access.
- Serial USB driver for your boards (macOS: usually none needed; Linux: add
  yourself to `dialout`: `sudo usermod -aG dialout $USER`, then re-login).

## 2. Get the code

The observer firmware lives on the `mqtt-uplink` branch of the fork:

```sh
git clone https://github.com/Stempit/MeshCore.git
cd MeshCore
git checkout mqtt-uplink
```

## 3. Build

First build downloads the toolchain and libraries — expect 5–15 minutes;
later builds are incremental.

**One board:**

```sh
pio run -e Heltec_v3_observer_meshcoretel
```

**All supported boards** (recommended — uses the release script):

```sh
./scripts/build-observer_meshcoretel.sh
```

The script: adds a git remote to upstream `meshcore-dev/MeshCore`, picks the
latest upstream `repeater-v*` release tag, rebases `mqtt-uplink` onto it,
builds every `*_observer_meshcoretel` environment, and collects the artifacts.

Successful build output looks like:

```
RAM:   [==        ]  20.6% (used 67432 bytes from 327680 bytes)
Flash: [====      ]  40.3% (used 1347317 bytes from 3342336 bytes)
========================= [SUCCESS] ...
```

If RAM or Flash shows > ~95%, stop and read
[add-new-device.md](add-new-device.md) § "Flash is full" — the board needs a
partition override.

## 4. Where the firmware is

```
out/observer_meshcoretel/<version>/<env>/
  firmware-merged.bin   ← flash this (bootloader + partitions + firmware)
  firmware.bin          ← app only
  bootloader.bin, partitions.bin
```

`<version>` is the upstream release number the firmware was built from (e.g.
`1.18.0`).

## 5. Flash a board

Plug the board in via USB and either let PlatformIO upload:

```sh
pio run -e Heltec_v3_observer_meshcoretel -t upload --upload-port /dev/ttyUSB0
```

(macOS ports look like `/dev/cu.usbmodem*`; on Windows `COMx`.)

or flash the merged image directly with esptool (chip name depends on the
board: `esp32`, `esp32s3`, `esp32c3`):

```sh
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
    write_flash 0x0 out/observer_meshcoretel/1.18.0/Heltec_v3_observer_meshcoretel/firmware-merged.bin
```

`firmware-merged.bin` writes bootloader, partition table and app in one go —
always prefer it over assembling offsets by hand.

⚠️ **Never flash `firmware.bin` alone on a device that ran other firmware.**
The repeater builds use OTA0/OTA1 partitions: a bare `firmware.bin` written to
one slot can be ignored by the bootloader, which keeps booting the *old*
firmware from the other slot (boot screen still shows the old version). If in
doubt, wipe first and flash the merged image:

```sh
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 erase_flash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
    write_flash 0x0 out/observer_meshcoretel/<version>/<env>/firmware-merged.bin
```

(`erase_flash` wipes the filesystem too — identity and settings are reset.)
To confirm you booted the right build: boot screen shows `v1.18.0`, and
`get mqtt.client_version` replies `observer_meshcoretel_v0.1.0`.

Browser flashing also works: on `flasher.meshcore.dev` select the board
**first**, then use its **Custom Firmware** option and pick our
`firmware-merged.bin`. Beware: the flasher's *normal* install flow always
downloads MeshCore's own stock build (currently v1.17.1 for repeaters) — it
never flashes your local file.

Then continue with [setup.md](setup.md).

## Troubleshooting

- **`pio: command not found`** — PlatformIO not on `PATH`; `pip install -U
  platformio` and reopen the shell.
- **Port not found / failed to upload** — wrong `--upload-port`, or the board
  is in use by a serial console; close it and retry. Hold the board's BOOT
  button while it tries if the bootloader isn't responding.
- **First build is slow / downloads fail** — PlatformIO fetches
  `framework-arduinoespressif32` and the Xtensa/RISC-V toolchains once; retry
  or check network/proxy.
- **Conflicts during the release rebase** — the script prints instructions;
  resolve `git rebase --continue` (or `--abort`) by hand and re-run.
