import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import RedarcRbus, redarc_rbus_ns

CODEOWNERS = ["@duarte-hub"]

CONF_REDARC_RBUS_ID = "redarc_rbus_id"

RedarcRbusButton = redarc_rbus_ns.class_("RedarcRbusButton", button.Button, cg.Component)

# One-shot actions the RedVision user app performs: (spn, index, value, dest pgn, fallback, icon).
ACTIONS = {
    # RBusManagerBloc.updateBatterySoc: SPN 21 (Battery State of Charge) = 100, sent to the
    # node that reports battery charge status (127236). Tells the monitor the battery is full.
    "sync_soc_full": (21, 0, 100, 127236, 0x08, "mdi:battery-sync"),
}

CONF_READ_CHANNEL_NAMES = "read_channel_names"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_REDARC_RBUS_ID): cv.use_id(RedarcRbus),
        # Read the output/channel names stored in the system, the way the app does (pauses
        # telemetry ~6 s). Results go to the log and the `channel_names` text sensor.
        cv.Optional(CONF_READ_CHANNEL_NAMES): button.button_schema(
            RedarcRbusButton, icon="mdi:tag-text-outline", entity_category="config"
        ).extend(cv.COMPONENT_SCHEMA),
        **{
            cv.Optional(key): button.button_schema(RedarcRbusButton, icon=a[5]).extend(
                cv.COMPONENT_SCHEMA
            )
            for key, a in ACTIONS.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_REDARC_RBUS_ID])
    if CONF_READ_CHANNEL_NAMES in config:
        var = await button.new_button(config[CONF_READ_CHANNEL_NAMES])
        await cg.register_component(var, config[CONF_READ_CHANNEL_NAMES])
        cg.add(var.set_parent(parent))
        cg.add(var.set_read_labels(True))
    for key, (spn, index, value, pgn, fallback, _) in ACTIONS.items():
        if key not in config:
            continue
        var = await button.new_button(config[key])
        await cg.register_component(var, config[key])
        cg.add(var.set_parent(parent))
        cg.add(var.set_parameter(spn, index, value))
        cg.add(var.set_destination(pgn, fallback))
