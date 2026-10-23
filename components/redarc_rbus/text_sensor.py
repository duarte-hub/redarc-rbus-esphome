import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import RedarcRbus

CODEOWNERS = ["@duarte-hub"]

CONF_REDARC_RBUS_ID = "redarc_rbus_id"
CONF_CHANNEL_STATUS = "channel_status"
CONF_CHANNEL = "channel"
CONF_CHANNEL_NAMES = "channel_names"
CONF_ACTIVE_FAULTS = "active_faults"

# History logs read from the Display's cache every `history_interval` (sensor hub option),
# published as "newest,...,oldest": (pgn, values per frame, bytes per value, max entries, icon).
LOGS = {
    "soc_hourly_log": (130256, 7, 1, 28, "mdi:chart-line"),       # %, last ~24 h
    "soc_daily_min_log": (130258, 7, 1, 35, "mdi:chart-line"),    # %, last ~30 days
    "soc_daily_max_log": (130260, 7, 1, 35, "mdi:chart-line"),    # %, last ~30 days
    "solar_daily_log": (130262, 3, 2, 30, "mdi:solar-power"),     # Wh per day
    "solar_hourly_log": (130261, 3, 2, 30, "mdi:solar-power"),    # Wh per hour
}

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
        # Result of the `read_channel_names` button: "4=Name; 5=Name; ...".
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(icon=l[4])
            for key, l in LOGS.items()
        },
        # Active faults reported by any node, with the app's titles, e.g. "Low Soc (0x8110)".
        cv.Optional(CONF_ACTIVE_FAULTS): text_sensor.text_sensor_schema(icon="mdi:alert-circle"),
        cv.Optional(CONF_CHANNEL_NAMES): text_sensor.text_sensor_schema(
            icon="mdi:tag-text-outline", entity_category="diagnostic"
        ),
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
    for key, (pgn, per_frame, nbytes, max_entries, _) in LOGS.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(parent.add_log_text_sensor(sens, pgn, per_frame, nbytes, max_entries))
    if CONF_ACTIVE_FAULTS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_ACTIVE_FAULTS])
        cg.add(parent.set_active_faults_sensor(sens))
    if CONF_CHANNEL_NAMES in config:
        sens = await text_sensor.new_text_sensor(config[CONF_CHANNEL_NAMES])
        cg.add(parent.set_labels_sensor(sens))
    for conf in config.get(CONF_CHANNEL_STATUS, []):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.add_text_sensor(sens, 130304, 2, 6, conf[CONF_CHANNEL], 7, 1,
                                      _table(CHANNEL_ORIGIN)))
