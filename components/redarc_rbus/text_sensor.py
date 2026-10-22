import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import RedarcRbus

CODEOWNERS = ["@duarte-hub"]

CONF_REDARC_RBUS_ID = "redarc_rbus_id"
CONF_CHANNEL_STATUS = "channel_status"
CONF_CHANNEL = "channel"

# Enum fields, names from the RedVision app's RBus*_Enum tables:
# (pgn, byte, bit, bits, {raw: name}, icon).
FIELDS = {
    "charging_stage": (127488, 0, 4, 4, {
        0: "Not Charging", 1: "Desulphation", 2: "Soft Start", 3: "Boost", 4: "Absorption",
        5: "Battery Test", 6: "Equalise", 7: "Float", 8: "Maintenance", 9: "Recovery",
        10: "Charge Back", 13: "Limp Mode (Battery Not Configured)",
    }, "mdi:battery-charging"),
    "charging_mode": (127488, 0, 0, 4, {0: "Touring", 1: "Storage"}, "mdi:caravan"),
    "battery_chemistry": (127232, 0, 0, 8, {
        0: "Gel", 1: "AGM", 2: "Flooded Lead Acid", 3: "Calcium Lead Acid", 4: "LiFePO4",
        5: "Heated LiFePO4",
    }, "mdi:flask"),
    "dc_input_trigger": (127494, 7, 0, 8, {
        0: "Automatic", 1: "12V", 2: "24V", 3: "Ignition", 4: "Solar", 5: "Always On",
        6: "Smart Alternator", 7: "Automatic LV",
    }, "mdi:car-cog"),
    "load_disconnect_trigger": (127240, 0, 2, 3, {
        0: "Loads Always Disabled", 1: "Battery Voltage", 2: "Battery SoC",
        3: "Loads Always Enabled", 4: "Local Supply Voltage",
    }, "mdi:power-plug-off"),
}

# Why an IO channel is in its current state: the 6-bit "origin" next to each channel's state
# in Digital IO Status (130304), names from RBusDigitalIOStateOrigin_Enum.
CHANNEL_ORIGIN = {
    0: "Normal", 1: "Fuse Failed / Not Present", 2: "Overcurrent / Overtemperature",
    3: "Not Connected", 4: "Overridden by User", 5: "Overridden by Logic",
    6: "Overridden by Load Disconnect", 7: "Overridden by Master Switch",
    8: "Overridden by Local Control", 9: "Overridden by Charging Mode",
    10: "Communications Error", 11: "Internal Error", 12: "Voltage Error", 13: "Current Error",
    14: "Temperature Error", 15: "Momentary Input Failure", 16: "Overridden by System Fault",
    62: "Channel Disabled",
}


def _table(names):
    return [names.get(i, "") for i in range(max(names) + 1)]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_REDARC_RBUS_ID): cv.use_id(RedarcRbus),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(icon=f[5])
            for key, f in FIELDS.items()
        },
        cv.Optional(CONF_CHANNEL_STATUS): cv.ensure_list(
            text_sensor.text_sensor_schema(icon="mdi:information-outline").extend(
                {cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=250)}
            )
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_REDARC_RBUS_ID])
    for key, (pgn, byte, bit, bits, names, _) in FIELDS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(parent.add_text_sensor(sens, pgn, byte * 8 + bit, bits, 0, 0, 0, _table(names)))
    for conf in config.get(CONF_CHANNEL_STATUS, []):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.add_text_sensor(sens, 130304, 2, 6, conf[CONF_CHANNEL], 7, 1,
                                      _table(CHANNEL_ORIGIN)))
