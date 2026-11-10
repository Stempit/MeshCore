# Building an update for all devices

When upstream [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore)
publishes a new release, rebuild the observer firmware for every supported
board and re-flash your nodes.

## Standard update flow

```sh
cd MeshCore
git checkout mqtt-uplink
./scripts/build-observer_meshcoretel.sh
```

With no arguments the script:

1. fetches upstream and picks the **latest `repeater-v*` tag**;
2. **rebases `mqtt-uplink` onto that release** — our fork diff is ~19 lines in
   3 files plus additive files, so this is almost always clean; on conflict it
   stops and prints exactly what to do (`git rebase --continue` after
   resolving, or `git rebase --abort` to give up);
3. builds **all 17** `*_observer_meshcoretel` environments;
4. writes fresh images to `out/observer_meshcoretel/<new-version>/<env>/`.

To target a specific release instead of the newest one:

```sh
./scripts/build-observer_meshcoretel.sh repeater-v1.19.0
```

One board only (quick smoke test before the full matrix):

```sh
./scripts/build-observer_meshcoretel.sh repeater-v1.19.0 --env Heltec_v3_observer_meshcoretel
```

## What to check before flashing the fleet

1. **Build sizes** — each env prints RAM/Flash usage; anything that jumped to
   > ~90% flash deserves a look.
2. **Behaviour of the new upstream release** — read the upstream release
   notes; radio/repeat behaviour changes affect observers too.
3. **One canary node first**: flash one representative board, verify
   `get mqtt.status` and packet flow on meshcoretel.ru, then roll out the rest.

## Re-flashing a node (fresh install)

The supported path is a **fresh/clean install** — it avoids every
firmware-compatibility question (old versions, OTA slots, partition layouts):

1. On `flasher.meshcore.dev` select the board **first**, then use
   **Custom Firmware** and pick `firmware-merged.bin` from the new version
   directory (see [build-from-scratch.md](build-from-scratch.md) § Flash a
   board).
2. Or locally:

   ```sh
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 erase_flash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
       write_flash 0x0 out/observer_meshcoretel/<version>/<env>/firmware-merged.bin
   ```

A clean install wipes the filesystem: the node generates a **new identity**
and all settings reset — re-run the [setup](setup.md) after flashing and
expect the node to re-appear under the same IATA but with a new pubkey.

## Keeping the fork and docs in sync

- If the rebase needed manual fixes, that's a signal: check whether upstream
  changed something our module relies on (ctor signature of `MyMesh`, CLI
  reply buffer sizes, etc.) and adjust `ObserverMesh`/`MQTTUplink`.
- Bump expected sizes / board list in this howto only if reality changed.
