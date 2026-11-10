# Setting up a flashed node

One-time configuration over the serial console (or remotely over the mesh via
admin CLI, if the node is already in your contact list). Settings persist
across reboots and firmware updates.

## 1. Connect to the node

Use the MeshCore companion app: the node advertises on boot and shows up in
the node list. Open it and log in as repeater admin — a fresh node uses the
default password `password`; change it right away with `password <new>`.
Then type the commands below in the repeater console.

Serial console (115200 baud) is the fallback when the node doesn't appear in
the app yet — e.g. when its radio settings don't match your region yet
(`set freq` / `set bw` / `set sf` / `set cr` are serial-only by design).

## 2. Configure WiFi, IATA, verify

Three commands do the job:

```
set wifi.ssid YourNetwork
set wifi.pwd YourPassword
set mqtt.iata KVX
```

- `iata` is your location code used in the MQTT topic
  (`meshcore/<IATA>/<pubkey>/...`); publishing stays off until it is set.
  Use the code your mesh has agreed on (for the Kirov mesh: `KVX`).
- The broker (meshcoretel) is **enabled by default**; you don't need to
  configure servers or credentials.

Then check:

```
get mqtt.status
```

Immediately after `set mqtt.iata` expect something like:

```
> wifi:up ntp:wait iata:KVX meshcoretel:wait eu:off us:off tx:off pkt:0 pub:0 drop:0 q:0/8 heap:118000
```

Within ~30–60 seconds it should become:

```
> wifi:up ntp:up iata:KVX meshcoretel:up eu:off us:off tx:off pkt:12 pub:12 drop:0 q:0/8 heap:110000
```

Field meanings: `wifi` — station link, `ntp` — clock synced (publishing is
gated until `up`), broker states — `up`/`wait`/`retry`/`backoff`/`off`/
`invalid-iata`, `pkt` — packets observed, `pub` — published, `drop` — lost
(queue full / broker outage > 60 s), `q` — offline queue depth, `heap` — free
RAM.

`pkt` climbing while `meshcoretel:up` means it works. Verify the node appears
on meshcoretel.ru under your IATA.

## 3. Optional settings

```
set mqtt.tx advert      # also publish own adverts (node visibility on maps)
set mqtt.raw on         # also publish raw frames (bigger uplink, by consent)
set mqtt.packets off    # stop publishing parsed packets (default on)
get wifi.status         # ip, rssi, signal quality
mqtt recover            # rebuild WiFi + broker connections from scratch
reboot                  # settings persist
```

Radio settings must match the regional settings of your mesh — follow the
meshcoretel.ru guidance for the region (KVX); do not copy radio parameters
from other regions' pages.

## Notes

- **Power**: an observer intentionally does not sleep (WiFi must stay up).
  Nodes with `powersaving_enabled` set stay awake too while the uplink is on.
- **Remote administration**: everything above works over the mesh from the
  app, e.g. `mqtt recover` from home. One limitation: `get mqtt.status`
  output is long and can be cut off over the mesh — read it over serial when
  you need the full line.
- **Security**: the WiFi password is stored on the device (not readable back
  over CLI); meshcoretel credentials are the public shared ones by broker
  design. Physical access to the node = access to its configuration.
