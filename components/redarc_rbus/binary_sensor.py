import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import RedarcRbus

CODEOWNERS = ["@duarte-hub"]

CONF_REDARC_RBUS_ID = "redarc_rbus_id"

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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_REDARC_RBUS_ID): cv.use_id(RedarcRbus),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(device_class=dc, icon=icon)
            for key, (_, _, _, dc, icon) in FIELDS.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_REDARC_RBUS_ID])
    for key, (pgn, byte, bit, _, _) in FIELDS.items():
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(parent.add_binary_sensor(sens, pgn, byte * 8 + bit, 2))
