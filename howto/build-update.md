# Building an update for all devices

When upstream [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore)
publishes a new release, rebuild the observer firmware for every supported
board and re-flash your nodes.

## Standard update flow

```sh
cd MeshCore
git checkout mqtt-uplink
./scripts/build-observer-meshcoretel.sh
```

With no arguments the script:

1. fetches upstream and picks the **latest `repeater-v*` tag**;
2. **rebases `mqtt-uplink` onto that release** — our fork diff is ~19 lines in
   3 files plus additive files, so this is almost always clean; on conflict it
   stops and prints exactly what to do (`git rebase --continue` after
   resolving, or `git rebase --abort` to give up);
3. builds **all 17** `*_observer_meshcoretel` environments;
4. writes fresh images to `out/observer-meshcoretel/<new-version>/<env>/`.

To target a specific release instead of the newest one:

```sh
./scripts/build-observer-meshcoretel.sh repeater-v1.19.0
```

One board only (quick smoke test before the full matrix):

```sh
./scripts/build-observer-meshcoretel.sh repeater-v1.19.0 --env Heltec_v3_observer_meshcoretel
```

## What to check before flashing the fleet

1. **Build sizes** — each env prints RAM/Flash usage; anything that jumped to
   > ~90% flash deserves a look.
2. **Behaviour of the new upstream release** — read the upstream release
   notes; radio/repeat behaviour changes affect observers too.
3. **One canary node first**: flash one representative board, verify
   `get mqtt.status` and packet flow on meshcoretel.ru, then roll out the rest.

## Re-flashing a node (update in place)

Flash `firmware-merged.bin` from the new version directory (see
[build-from-scratch.md](build-from-scratch.md) § Flash a board).

**Settings survive** a firmware update as long as the partition layout is the
same as the previously installed firmware: node identity, repeater settings
(`/prefs.json`) and observer settings (`/mqtt_prefs`: wifi, iata, broker
switches) all live in the SPIFFS partition, which a `firmware-merged.bin`
flash rewrites only when the partition table itself changed.

⚠️ **Exception — T-Beam 1W and Generic E22** (and any board you gave a
`min_spiffs.csv` override): their observer builds use a different partition
table than stock MeshCore repeater firmware. Converting such a node **from
stock firmware to the observer build** (or back) reformats the filesystem —
write down your settings first (`get wifi.ssid`, `get mqtt.iata`) and expect
to run the [setup](setup.md) again. Updates **between observer builds** keep
settings normally.

## Keeping the fork and docs in sync

- If the rebase needed manual fixes, that's a signal: check whether upstream
  changed something our module relies on (ctor signature of `MyMesh`, CLI
  reply buffer sizes, etc.) and adjust `ObserverMesh`/`MQTTUplink`.
- Bump expected sizes / board list in this howto only if reality changed.
