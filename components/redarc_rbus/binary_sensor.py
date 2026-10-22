import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import RedarcRbus

CODEOWNERS = ["@duarte-hub"]

CONF_REDARC_RBUS_ID = "redarc_rbus_id"
CONF_DIGITAL_CHANNELS = "digital_channels"
CONF_IO_ALARMS = "io_alarms"
CONF_CHANNEL = "channel"

# 2-bit J1939 booleans (0 = off, 1 = on, 2 = error, 3 = n/a), from the RedVision app's PGN
# catalogue: (pgn, byte, bit, device_class, icon).
FIELDS = {
    "ac_input_active": (127492, 6, 0, "power", "mdi:power-plug"),
    "dc_input_active": (127494, 6, 0, "power", "mdi:car-battery"),
    "solar_input_active": (127496, 6, 0, "power", "mdi:solar-power"),
    "charger_derating": (127498, 6, 0, "problem", "mdi:thermometer-alert"),
    "inverter_on": (130288, 0, 0, "power", "mdi:current-ac"),
    "low_battery_soc_alarm": (127242, 0, 0, "problem", "mdi:battery-alert"),
    "low_battery_voltage_alarm": (127242, 0, 2, "problem", "mdi:battery-alert"),
    "load_disconnect_active": (127240, 0, 0, "problem", "mdi:power-plug-off"),
}

# Per-IO-channel PGNs: byte 0 = first channel in the frame, then 7 one-byte slots, each with a
# 2-bit state in bits 0-1. (pgn, per_frame, slot_bytes)
DIGITAL_IO_STATUS = (130304, 7, 1)  # on/off state of outputs and digital inputs
IO_ALARM_STATUS = (130311, 7, 1)  # tank / analog over-under alarms

_CHANNEL_SCHEMA = {cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=250)}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_REDARC_RBUS_ID): cv.use_id(RedarcRbus),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(device_class=dc, icon=icon)
            for key, (_, _, _, dc, icon) in FIELDS.items()
        },
        cv.Optional(CONF_DIGITAL_CHANNELS): cv.ensure_list(
            binary_sensor.binary_sensor_schema(icon="mdi:electric-switch").extend(_CHANNEL_SCHEMA)
        ),
        cv.Optional(CONF_IO_ALARMS): cv.ensure_list(
            binary_sensor.binary_sensor_schema(device_class="problem").extend(_CHANNEL_SCHEMA)
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_REDARC_RBUS_ID])
    for key, (pgn, byte, bit, _, _) in FIELDS.items():
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(parent.add_binary_sensor(sens, pgn, byte * 8 + bit, 2, 0, 0, 0))
    for key, (pgn, per_frame, slot_bytes) in (
        (CONF_DIGITAL_CHANNELS, DIGITAL_IO_STATUS),
        (CONF_IO_ALARMS, IO_ALARM_STATUS),
    ):
        for conf in config.get(key, []):
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(parent.add_binary_sensor(sens, pgn, 0, 2, conf[CONF_CHANNEL], per_frame,
                                            slot_bytes))
