"""REDARC RedVision TVMS R-Bus BLE proxy for ESPHome.

Reads battery telemetry (State of Charge, voltage) from a REDARC RedVision
DISP4300 Display over Bluetooth Low Energy and exposes it as ESPHome sensors,
and can switch outputs by replaying captured R-Bus command frames.

The component connects as a *bonding* BLE central: it pairs once with the
6-digit passkey shown on the Display's Settings -> Bluetooth screen, then
reconnects silently using the stored bond.
"""

import esphome.codegen as cg
from esphome.components import ble_client

CODEOWNERS = ["@duarte-hub"]

redarc_rbus_ns = cg.esphome_ns.namespace("redarc_rbus")
RedarcRbus = redarc_rbus_ns.class_("RedarcRbus", cg.Component, ble_client.BLEClientNode)
