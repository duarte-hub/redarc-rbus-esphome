import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import RedarcRbus, redarc_rbus_ns

CODEOWNERS = ["@duarte-hub"]
# The dependency on the hub is enforced by the `redarc_rbus_id` reference below
# (cv.use_id), not by a top-level component, so no DEPENDENCIES entry here.

CONF_REDARC_RBUS_ID = "redarc_rbus_id"
CONF_ON_FRAME = "on_frame"
CONF_OFF_FRAME = "off_frame"
CONF_CHANNEL = "channel"
CONF_NODE_ADDRESS = "node_address"

RedarcRbusSwitch = redarc_rbus_ns.class_(
    "RedarcRbusSwitch", switch.Switch, cg.Component
)


def _hex_frame(value):
    """A raw R-Bus command frame as hex (e.g. '2f0024fa08cb00030701000000')."""
    s = cv.string_strict(value).replace(" ", "").replace(":", "")
    try:
        data = bytes.fromhex(s)
    except ValueError as err:
        raise cv.Invalid("on_frame/off_frame must be hex bytes") from err
    if not 1 <= len(data) <= 20:
        raise cv.Invalid("frame must be 1..20 bytes")
    return list(data)


def _one_mode(config):
    frames = CONF_ON_FRAME in config or CONF_OFF_FRAME in config
    if CONF_CHANNEL in config and frames:
        raise cv.Invalid("use either channel or on_frame/off_frame, not both")
    if CONF_CHANNEL not in config and not (CONF_ON_FRAME in config and CONF_OFF_FRAME in config):
        raise cv.Invalid("set channel (recommended) or both on_frame and off_frame")
    return config


CONFIG_SCHEMA = cv.All(
    switch.switch_schema(RedarcRbusSwitch)
    .extend(
        {
            cv.GenerateID(CONF_REDARC_RBUS_ID): cv.use_id(RedarcRbus),
            # IO channel number of the output (e.g. 7 for the Main Light on a TVMS). Switches it
            # the way the app does and follows the real state from the Distribution Box.
            cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=250),
            # R-Bus address of the node that owns the channel; learned automatically.
            cv.Optional(CONF_NODE_ADDRESS): cv.hex_int_range(min=0, max=0xFD),
            cv.Optional(CONF_ON_FRAME): _hex_frame,
            cv.Optional(CONF_OFF_FRAME): _hex_frame,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _one_mode,
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_REDARC_RBUS_ID])
    cg.add(var.set_parent(parent))
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
        if CONF_NODE_ADDRESS in config:
            cg.add(var.set_node_address(config[CONF_NODE_ADDRESS]))
        # Read-back: Digital IO Status (130304), 7 one-byte slots, state in bits 0-1.
        cg.add(parent.add_listener(var, 130304, 0, 2, config[CONF_CHANNEL], 7, 1))
    else:
        cg.add(var.set_on_frame(config[CONF_ON_FRAME]))
        cg.add(var.set_off_frame(config[CONF_OFF_FRAME]))
