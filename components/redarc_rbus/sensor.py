import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import ble_client, sensor
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT

from . import RedarcRbus

CODEOWNERS = ["@duarte-hub"]
DEPENDENCIES = ["ble_client", "esp32_ble"]

CONF_PASSKEY = "passkey"
CONF_STATUS_LED = "status_led"
CONF_RSSI = "rssi"
CONF_RSSI_UPDATE_INTERVAL = "rssi_update_interval"
CONF_BATTERY_POWER = "battery_power"
CONF_SOLAR_POWER = "solar_power"

# R-Bus fields, from the RedVision app's generated PGN catalogue (see pgn_table.md in the
# research repo). Each: (pgn, start byte, start bit, bits, offset, scale) then entity defaults
# (unit, device_class, accuracy_decimals, entity_category). Value = (raw + offset) * scale.
# Device class / unit strings are literals: some esphome.const names are missing on older releases.
_I32 = dict(bits=32, offset=-1000000, scale=0.001)  # signed current, mA with 1e6 bias
_MV = dict(bits=16, offset=0, scale=0.001)  # millivolts
_AVG_A = dict(bits=16, offset=-10000, scale=0.1)  # averaged current, 0.1 A with 1e4 bias


def _f(pgn, byte, bit=0, bits=8, offset=0, scale=1.0, unit=None, dc=None, dec=0, diag=False, icon=None):
    return dict(pgn=pgn, byte=byte, bit=bit, bits=bits, offset=offset, scale=scale,
                unit=unit, dc=dc, dec=dec, diag=diag, icon=icon)


FIELDS = {
    # Battery Charge Status (127236)
    "state_of_charge": _f(127236, 0, unit="%", dc="battery"),
    "state_of_health": _f(127236, 1, unit="%", icon="mdi:battery-heart-variant"),
    # Signed: positive = minutes until full, negative = minutes until flat.
    "time_remaining": _f(127236, 2, bits=16, offset=-32127, scale=5, unit="min",
                         icon="mdi:timer-sand"),
    # Battery Sensor Measurements (127234)
    "voltage": _f(127234, 4, **_MV, unit="V", dc="voltage", dec=3),
    "current": _f(127234, 0, **_I32, unit="A", dc="current", dec=2),
    "temperature": _f(127234, 6, offset=-60, unit="°C", dc="temperature"),
    # Battery Sensor Measurements - Averaged (127616)
    "load_current": _f(127616, 4, **_AVG_A, unit="A", dc="current", dec=1),
    # Charger inputs / output (127492 AC, 127494 DC, 127496 solar, 127498 output)
    "ac_input_voltage": _f(127492, 4, bits=16, unit="V", dc="voltage"),
    "ac_input_current": _f(127492, 0, **_I32, unit="A", dc="current", dec=2),
    "dc_input_voltage": _f(127494, 4, **_MV, unit="V", dc="voltage", dec=2),
    "dc_input_current": _f(127494, 0, **_I32, unit="A", dc="current", dec=2),
    "solar_voltage": _f(127496, 4, **_MV, unit="V", dc="voltage", dec=2),
    "solar_current": _f(127496, 0, **_I32, unit="A", dc="current", dec=2),
    "charger_output_voltage": _f(127498, 4, **_MV, unit="V", dc="voltage", dec=2),
    "charger_output_current": _f(127498, 0, **_I32, unit="A", dc="current", dec=2),
    # Inverter Status (130288); power field is kW x 0.001, i.e. raw watts
    "inverter_output_voltage": _f(130288, 2, bits=16, unit="V", dc="voltage"),
    "inverter_power": _f(130288, 4, bits=16, unit="W", dc="power"),
    # Alarm / load-disconnect set-points (127242, 127240)
    "low_battery_soc_setpoint": _f(127242, 1, unit="%", diag=True),
    "low_battery_voltage_setpoint": _f(127242, 2, **_MV, unit="V", dc="voltage", dec=2, diag=True),
    "load_disconnect_voltage_off": _f(127240, 1, **_MV, unit="V", dc="voltage", dec=2, diag=True),
    "load_disconnect_voltage_on": _f(127240, 3, **_MV, unit="V", dc="voltage", dec=2, diag=True),
    "load_disconnect_soc_off": _f(127240, 5, unit="%", diag=True),
    "load_disconnect_soc_on": _f(127240, 6, unit="%", diag=True),
    # Battery / charger configuration (127232, 128008)
    "battery_capacity": _f(127232, 1, bits=16, unit="Ah", diag=True, icon="mdi:battery"),
    "battery_max_charge_current": _f(127232, 4, unit="A", dc="current", diag=True),
    "charger_max_output_current": _f(128008, 0, bits=16, scale=0.1, unit="A", dc="current",
                                     dec=1, diag=True),
}


def _field_schema(f):
    kw = dict(accuracy_decimals=f["dec"])
    if f["unit"]:
        kw["unit_of_measurement"] = f["unit"]
    if f["dc"]:
        kw["device_class"] = f["dc"]
    if f["icon"]:
        kw["icon"] = f["icon"]
    if f["diag"]:
        kw["entity_category"] = "diagnostic"
    else:
        kw["state_class"] = STATE_CLASS_MEASUREMENT
    return sensor.sensor_schema(**kw)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RedarcRbus),
            # 6-digit passkey shown on the Display's Settings -> Bluetooth screen.
            cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
            # Optional activity LED, pulsed while BLE frames flow (e.g. onboard GPIO2).
            cv.Optional(CONF_STATUS_LED): pins.gpio_output_pin_schema,
            **{cv.Optional(key): _field_schema(f) for key, f in FIELDS.items()},
            # Derived from V x I in the same frame; negative battery power = discharging.
            cv.Optional(CONF_BATTERY_POWER): sensor.sensor_schema(
                unit_of_measurement="W",
                accuracy_decimals=1,
                device_class="power",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SOLAR_POWER): sensor.sensor_schema(
                unit_of_measurement="W",
                accuracy_decimals=1,
                device_class="power",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement="dBm",
                accuracy_decimals=0,
                device_class="signal_strength",
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category="diagnostic",
            ),
            # How often to poll connection RSSI (set 0s to disable polling).
            cv.Optional(
                CONF_RSSI_UPDATE_INTERVAL, default="60s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if CONF_PASSKEY in config:
        cg.add(var.set_passkey(config[CONF_PASSKEY]))
    if CONF_STATUS_LED in config:
        pin = await cg.gpio_pin_expression(config[CONF_STATUS_LED])
        cg.add(var.set_status_led(pin))
    for key, f in FIELDS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            shift = f["byte"] * 8 + f["bit"]
            cg.add(var.add_sensor(sens, f["pgn"], shift, f["bits"], f["offset"], f["scale"]))
    if CONF_BATTERY_POWER in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_POWER])
        cg.add(var.set_battery_power_sensor(sens))
    if CONF_SOLAR_POWER in config:
        sens = await sensor.new_sensor(config[CONF_SOLAR_POWER])
        cg.add(var.set_solar_power_sensor(sens))
    if CONF_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(var.set_rssi_sensor(sens))
    cg.add(var.set_rssi_interval(config[CONF_RSSI_UPDATE_INTERVAL].total_milliseconds))
