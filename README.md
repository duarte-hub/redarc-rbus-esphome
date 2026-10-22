# REDARC RedVision → ESPHome

An [ESPHome](https://esphome.io) external component that reads battery, charger,
inverter and alarm telemetry from a **REDARC RedVision TVMS** (DISP4300 Display)
over Bluetooth Low Energy and exposes it to Home Assistant — no wiring into the
R-Bus CAN harness required. See [Entities](#entities) for the full list.

It can also **switch outputs** on/off (e.g. lights) by replaying the exact R-Bus
command frames the RedVision app sends — see *Output control* below.

This was built by reverse-engineering the RedVision app and the Display's BLE
interface for **interoperability with hardware the owner already owns**. It is
read-only unless you configure `switch:` entities: it never writes configuration,
only the filter frames needed to make the gateway stream telemetry.

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

**`binary_sensor:`** (platform `redarc_rbus`, with `redarc_rbus_id:`):
`ac_input_active`, `dc_input_active`, `solar_input_active`, `charger_derating`,
`inverter_on`, `low_battery_soc_alarm`, `low_battery_voltage_alarm`,
`load_disconnect_active`.

**`text_sensor:`** (platform `redarc_rbus`, with `redarc_rbus_id:`):
`charging_stage` (Not Charging / Boost / Absorption / Float / …), `charging_mode`
(Touring / Storage), `battery_chemistry`, `dc_input_trigger`,
`load_disconnect_trigger`.

See [`example.yaml`](example.yaml) for every entity configured.

## Output control (switches)

The TVMS switches loads when the app writes a `power_port_control` /
`lighting_control_command` R-Bus frame to char 8001. These are plaintext and
need no extra authentication beyond the bond. Rather than guess the channel
encoding, the `switch` platform **replays the exact bytes** the app sends, so it
can only ever do what the real app did — no risk of hitting the wrong channel.

```yaml
switch:
  - platform: redarc_rbus
    redarc_rbus_id: redarc_hub        # id of your sensor-platform hub
    name: "Main Light"
    on_frame:  "2f0024fa08cb00030701000000"
    off_frame: "2f0024fa08cb00030700000000"
```

### Capturing frames for your outputs

`on_frame` / `off_frame` are the 13-byte R-Bus frames written to char 8001.
To get them for any output:

1. Take a PacketLogger capture (as for telemetry) while you toggle **one** output
   ON then OFF in the RedVision app.
2. Pull the two writes to handle `0x0020` that are unique and ~seconds apart:
   ```
   tshark -r capture.pklg -Y "btatt.opcode==0x52 && btatt.handle==0x0020" \
          -T fields -e frame.time_relative -e btatt.value
   ```
   The ON/OFF pair differs by a single byte. Paste each as a hex string.

The switch is an *assumed-state* switch (it shows the last commanded state); live
read-back from the Digital IO Status PGN (130304) is on the roadmap.

> Safety: these frames command a power-distribution box. Only use frames you
> captured from your own unit, and label each switch clearly.

## Status / roadmap

- ✅ State of Charge, voltage — decoded and verified against the live display.
- ✅ Battery current/temperature/power, time remaining, solar/DC/AC inputs, charger
  output, inverter, alarms, load disconnect, charging stage/mode — field layouts
  from the app's PGN catalogue, checked against a real capture.
- ✅ Output control — switch outputs on/off by replaying captured command frames.
- ⏳ Tank levels (analog IO channels; needs per-channel scaling).
- ⏳ Live output state read-back (Digital IO Status, PGN 130304) to drive non-assumed switches.
- ⏳ Channel-based switch config (specify an output number instead of raw frames),
  once a few more captured samples confirm the channel encoding.

## Legal

For personal interoperability with your own equipment. "REDARC" and "RedVision"
are trademarks of REDARC Electronics; this project is not affiliated with or
endorsed by REDARC. No REDARC firmware or app binaries are redistributed here.
