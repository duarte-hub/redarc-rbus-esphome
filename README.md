# REDARC RedVision → ESPHome

An [ESPHome](https://esphome.io) external component that reads battery, charger,
inverter and alarm telemetry from a **REDARC RedVision TVMS** (DISP4300 Display)
over Bluetooth Low Energy and exposes it to Home Assistant — no wiring into the
R-Bus CAN harness required. See [Entities](#entities) for the full list.

It can also **switch outputs** on/off (e.g. lights) by replaying the exact R-Bus
command frames the RedVision app sends — see *Output control* below.

This was built by reverse-engineering the RedVision app and the Display's BLE
interface for **interoperability with hardware the owner already owns**. It is
read-only unless you configure `switch:`, `select:` or `button:` entities, and those
only send the same "set parameter" commands the RedVision user app sends.

## How it works

The Display runs a u-blox NINA-B1 BLE module exposing a custom GATT service
(`09022015-…-524544415243`, tail = ASCII "REDARC"). Telemetry is
**R-Bus (J1939-over-CAN)** — 8-byte CAN frames tunnelled over BLE as 13-byte
notifications on characteristic `…8001`:

```
[ 4-byte J1939 29-bit CAN id ][ DLC=0x08 ][ 8-byte payload ]
```

The Display only streams to a **bonded** central. Enabling notifications returns
an authentication error, which forces **legacy passkey pairing** — the 6-digit
code on the Display's *Settings → Bluetooth* screen. This component:

1. Pairs once using that passkey (IO capability = *keyboard only*) and stores the bond.
2. Subscribes to `…8001` and replays the filter-enrollment writes to `…8002`
   that tell the gateway which PGNs to forward.
3. Decodes the battery PGNs and publishes them as sensors.

After the first successful pairing the bond is saved in the ESP32's flash, so it
reconnects silently on every boot.

Field decode follows the app's own parser, which amounts to little-endian
extraction: `raw = (payload_le_u64 >> (byte*8 + bit)) & mask`, then
`value = (raw + offset) * scale`. Raw values in the J1939 "not available /
error" range are dropped. Field positions, offsets and scales come from the
app's generated PGN catalogue.

## Install

1. Add the component and BLE stack to your ESPHome config (see
   [`example.yaml`](example.yaml)):

   ```yaml
   external_components:
     - source:
         type: git
         url: https://github.com/duarte-hub/redarc-rbus-esphome
       components: [redarc_rbus]

   esp32_ble_tracker:
   esp32_ble:
     io_capability: keyboard_only

   ble_client:
     - mac_address: D4:CA:6E:B6:1E:5F   # your Display's u-blox module MAC
       id: redarc_display

   sensor:
     - platform: redarc_rbus
       ble_client_id: redarc_display
       passkey: 123456                  # the code shown on Settings -> Bluetooth
       state_of_charge:
         name: "Battery State of Charge"
       voltage:
         name: "Battery Voltage"
   ```

2. **Requirements**
   - An **ESP32 with Bluetooth** (e.g. `esp32dev`).
   - **`framework: type: esp-idf`** — the Arduino framework doesn't expose the
     passkey/bonding GAP calls this uses.
   - The Display's BLE **MAC address**. The default above is a u-blox NINA-B1
     address; if yours differs, run an `esp32_ble_tracker:` scan and look for
     `REDARC DISP4300 …` in the logs.
   - The **6-digit passkey**. Open *Settings → Bluetooth* on the Display to show
     it. (The Display advertises only while that screen is open, so keep it open
     for the very first pairing.)

## First-time pairing

1. Put the Display on its *Settings → Bluetooth* screen so it is discoverable and
   shows the passkey.
2. Set that passkey in your config and flash the ESP32.
3. Watch the ESPHome logs — you should see `Passkey requested…` then
   `Bonded with Display`, followed by `Filter enrollment complete` and sensor
   updates.

If pairing fails with a reason code, double-check the passkey. Only one central
can be connected at a time, so close the phone app while the ESP32 connects.

## The bond survives reflashes; the rotating code only matters at pairing time

After pairing, the BLE bond is stored in the ESP32's **NVS flash partition**. A
normal `esphome run` / OTA update overwrites only the *app* partition, so the
bond — and therefore the connection — **survives firmware updates**. You pair
once and the rotating passkey never matters again. You only need to re-pair after
a **full chip erase** (`esptool erase_flash`), so avoid that and prefer OTA.

## Re-pairing with a rotating code, without recompiling

If your Display shows a **fresh passkey each time** (many do), you don't want to
edit YAML and reflash every time you need to re-pair. Add a runtime input and a
button so you enter the current code from Home Assistant:

```yaml
sensor:
  - platform: redarc_rbus
    id: redarc_hub                 # <-- give the component an id
    ble_client_id: redarc_display
    passkey: !secret redarc_passkey # optional fallback for the very first boot
    state_of_charge:
      name: "Battery State of Charge"
    voltage:
      name: "Battery Voltage"

number:
  - platform: template
    name: "REDARC Pairing Code"
    id: redarc_passkey_input
    min_value: 0
    max_value: 999999
    step: 1
    mode: box
    optimistic: true
    restore_value: true            # remembered across reboots (kept in NVS)
    set_action:
      - lambda: "id(redarc_hub).set_passkey((uint32_t) x);"

button:
  - platform: template
    name: "REDARC Pair Now"
    on_press:
      - lambda: "id(redarc_hub).pair_now();"
```

Because the code **rotates**, the component waits at the Display's passkey prompt
(for ~30 s) for you to supply the code that is showing *at that moment*, instead
of sending a stale one. Reliable sequence:

1. Open the Display's *Settings → Bluetooth* screen (and close the phone app —
   also "Forget This Device" on the phone so it doesn't grab the single slot).
2. Press **REDARC Pair Now**. The ESP reconnects; the log shows
   `Display is requesting pairing. Enter the 6-digit code it is showing NOW…`.
3. Read the code the Display shows **now** and type it into **REDARC Pairing
   Code** in HA. The component answers immediately → `Bonded with Display`.

(If you instead enter the code first and press Pair Now within ~25 s, that also
works — the component treats a just-entered code as fresh and answers at once.)
Once bonded, the keys are stored and you're set until the next full erase.

## Configuration reference

`sensor:` platform `redarc_rbus`:

| Option | Required | Description |
| --- | --- | --- |
| `ble_client_id` | yes | ID of the `ble_client:` entry for the Display |
| `passkey` | recommended | 6-digit pairing code shown on the Display |
| `status_led` | no | GPIO pin pulsed on BLE traffic |
| `rssi` / `rssi_update_interval` | no | BLE link RSSI sensor, poll interval (default 60s) |
| `min_publish_interval` | no | Changed sensor values go to HA at most this often (default `5s`, `0s` = every change). Switches, selects and text stay instant |
| `keep_awake` | no | Send the app's Keep Awake frame every 60 s while connected (default `true`). In Storage mode the TVMS sleeps by design and stops streaming; the ESP stays connected (reconnecting to a sleeping system made the Display demand a new passkey) and streaming resumes when it wakes. `false` = never send Keep Awake |
| `history_interval` | no | How often to read the Display's cached history logs (default `15min`, `0s` = never) |
| `load_disconnect_node` | no | Load Disconnect Status is sent by both the charger (`0x01`) and the distribution box (`0x24`) with different set-points; entities follow this node (default `0x24`) |
| `connection_interval` | no | Experimental: BLE connection interval to request once streaming (default `0ms` = leave as negotiated) |
| *any key in the sensor table below* | no | Sensor schema |

## Entities

All are optional; add only the ones you want. Which ones report data depends on
the hardware on your R-Bus (e.g. no inverter → inverter entities stay unknown).
Each entity publishes when its value changes, and at least once a minute
otherwise. Values like voltage change on most frames, so add a
`throttle_average` filter if you want fewer updates.

**`sensor:`** (platform `redarc_rbus`, the hub):

| Key | Unit | PGN | Notes |
| --- | --- | --- | --- |
| `state_of_charge` | % | 127236 | |
| `state_of_health` | % | 127236 | not reported by every battery |
| `time_remaining` | min | 127236 | positive = to full, negative = to flat |
| `voltage` | V | 127234 | battery |
| `current` | A | 127234 | negative = discharging |
| `temperature` | °C | 127234 | battery sensor |
| `battery_power` | W | 127234 | derived V × I |
| `load_current` | A | 127616 | averaged |
| `solar_voltage` / `solar_current` | V / A | 127496 | |
| `solar_power` | W | 127496 | derived V × I |
| `dc_input_voltage` / `dc_input_current` | V / A | 127494 | alternator / DC-DC input |
| `ac_input_voltage` / `ac_input_current` | V / A | 127492 | mains charger input |
| `charger_output_voltage` / `charger_output_current` | V / A | 127498 | |
| `inverter_output_voltage` / `inverter_power` | V / W | 130288 | |
| `battery_capacity` | Ah | 127232 | diagnostic |
| `battery_max_charge_current` | A | 127232 | diagnostic |
| `charger_max_output_current` | A | 128008 | diagnostic |
| `low_battery_soc_setpoint` / `low_battery_voltage_setpoint` | % / V | 127242 | diagnostic |
| `load_disconnect_voltage_off` / `_on` | V | 127240 | diagnostic |
| `load_disconnect_soc_off` / `_on` | % | 127240 | diagnostic |
| `solar_energy_today` / `solar_energy_yesterday` | Wh | 130262 | daily solar log |
| `soc_min_last_day` / `soc_max_last_day` | % | 130258 / 130260 | last completed day |
| `analog_channels:` (list of `channel:` + sensor) | raw | 130306 | tank levels etc., see below |

**`binary_sensor:`** (platform `redarc_rbus`, with `redarc_rbus_id:`):
`ac_input_active`, `dc_input_active`, `solar_input_active`, `charger_derating`,
`inverter_on`, `low_battery_soc_alarm`, `low_battery_voltage_alarm`,
`load_disconnect_active`, `fault_active` (any device reports an active fault); plus
lists of `channel:` entries:
`digital_channels` (live state of any IO channel, PGN 130304) and `io_alarms`
(analog channel over/under alarm, PGN 130311).

**`text_sensor:`** (platform `redarc_rbus`, with `redarc_rbus_id:`):
`charging_stage` (Not Charging / Boost / Absorption / Float / …), `charging_mode`
(Touring / Storage), `battery_chemistry`, `dc_input_trigger`,
`load_disconnect_trigger`; plus `channel_status` (list of `channel:` entries): why an
output is in its state (Normal, Fuse Failed, Overridden by Load Disconnect, …).

- `active_faults`: active faults from any device, using the app's fault titles,
  e.g. `Low Soc (0x8110)`, or `None`. All 71 codes the app knows are built in.
- History logs, read from the Display every `history_interval` and published as
  `newest,…,oldest`: `soc_hourly_log` (24 h of %), `soc_daily_min_log` /
  `soc_daily_max_log` (~30 days of %), `solar_daily_log` (Wh per day),
  `solar_hourly_log` (Wh per hour, not every system has it). Use a template sensor or
  an ApexCharts `data_generator` to chart them.
- `channel_names`: result of the `read_channel_names` button, e.g.
  `4=Tent Light; 5=Left Light; …`.

**`select:`** (platform `redarc_rbus`, with `redarc_rbus_id:`): `charging_mode`
(Touring / Storage) and `dc_input_trigger` — settings the app changes; they follow the
device's reported value.

**`button:`** (platform `redarc_rbus`, with `redarc_rbus_id:`): `sync_soc_full` — tells
the battery monitor the battery is full (the app's "sync SoC to 100 %");
`read_channel_names` — reads the output/input names stored in the system (see below).

### Reading channel names

Press `read_channel_names` to read the names your installer gave each channel
(e.g. "Main Light", "Water Pump"). It replays the RedVision app's own channel-detail
read: the gateway is switched to IO-only filters, one label request goes to the IO
node, replies are collected for 5 s, and the standard filters are always restored.
Telemetry pauses for ~6 s. Names appear in the log (`IO channel 7 label: "Main Light"`)
and the `channel_names` text sensor — copy them into your switch names.

> An earlier automatic version (a bare label request with the normal filters active)
> stopped the stream on a DISP4300 and made it forget its pairing. The current
> sequence matches captured app traffic byte for byte and has been run successfully,
> but it only ever runs when you press the button.

### History logs

The Display keeps the SoC and solar logs; they are not streamed on the bus. The
component asks for the Display's cached copy with the app's 5-byte "request to read"
frames (`2f fc d0 fa 48`, …), which the Display answers itself without CAN traffic.

### IO channels

A TVMS numbers all IO from 1: outputs and digital inputs first (1–16 on the
distribution box), then analog channels (17+, e.g. tank probes on 21 and 22).
Analog values are raw: tank probes read in % (0/25/50/75/100 for a 5-pin probe),
other channels may be millivolts — compare with the Display and add a
`unit_of_measurement` / `filters` to suit.

See [`example.yaml`](example.yaml) for every entity configured.

## Link diagnostics

Optional entities to explain dropouts and lost bonds. They are published the moment
something happens, so HA's history becomes a timeline:

| Platform | Key | Shows |
|---|---|---|
| text_sensor | `link_status` | Searching / Connected, securing link / Pairing required / Bonded, starting stream / Streaming / Connected, system asleep / Disconnected |
| text_sensor | `last_disconnect` | Reason (e.g. supervision timeout, Display closed the link, key missing), how long the link had been up, the state it was in, time since the last frame |
| text_sensor | `last_pairing` | When the Display asked for a code and whether the ESP still had the bond (if yes, the Display forgot it), or why pairing failed |
| binary_sensor | `pairing_required` | On while the Display waits for its 6-digit code: use it to send a phone notification |
| sensor | `disconnects`, `pairing_requests` | Counts since the ESP booted |

Add ESPHome's `debug` (`reset_reason`) and `uptime` sensors too, to see why and when the ESP restarted.

## Output control (switches)

Outputs are switched exactly the way the app's control screen does it: an
**RBUS Parameter Set** (PGN 65536) for SPN 203 (*Digital IO State*) with the IO
channel as the index and 1/0 as the value, sent to the node that owns the channel.
The switch then follows the real state from Digital IO Status (PGN 130304), so
changes made on the Display or in the app show up in Home Assistant too.

```yaml
switch:
  - platform: redarc_rbus
    redarc_rbus_id: redarc_hub        # id of your sensor-platform hub
    name: "Main Light"
    channel: 7
```

To find an output's channel, toggle it in the RedVision app while watching the
ESPHome logs: the node acknowledges with
`Ack from node 0x24: type 1 SPN 203 index 7 status 0` (index = channel). The
destination node is learned from the bus; set `node_address:` to override it.

The older raw-frame mode (`on_frame` / `off_frame`, assumed state) still works:
`2f0024fa08cb00030701000000` is exactly the frame `channel: 7` builds for ON.

> Safety: these commands switch a power-distribution box. Label each switch
> clearly and only map channels you have identified on your own unit.

## Settings

`select:` and `button:` entities send only the settings the RedVision *user* app
itself changes, with the same SPN, index and destination:

| Entity | SPN | Sent to |
| --- | --- | --- |
| `charging_mode` | 67 | broadcast (0xFF), as the app does |
| `dc_input_trigger` | 104 | the charger node (sender of PGN 127494) |
| `sync_soc_full` | 21 = 100 | the battery monitor (sender of PGN 127236) |

Thresholds such as low-battery alarms and load-disconnect points are only changed
by REDARC's installer app, so they are exposed read-only.

## Status / roadmap

- ✅ State of Charge, voltage — decoded and verified against the live display.
- ✅ Battery current/temperature/power, time remaining, solar/DC/AC inputs, charger
  output, inverter, alarms, load disconnect, charging stage/mode — field layouts
  from the app's PGN catalogue, checked against a real capture.
- ✅ Output control by channel with live read-back, and per-channel status/origin.
- ✅ Analog channels (tank levels), IO alarms, daily solar Wh, SoC history.
- ✅ Channel names read from the system; active faults with the app's descriptions;
  hourly/daily SoC and solar history logs.
- ✅ Charging mode, DC input trigger, SoC sync — the user app's own settings.
- ⏳ Dimmable / RGBW outputs (not present on the developer's unit, so unverified).

## Legal

For personal interoperability with your own equipment. "REDARC" and "RedVision"
are trademarks of REDARC Electronics; this project is not affiliated with or
endorsed by REDARC. No REDARC firmware or app binaries are redistributed here.
