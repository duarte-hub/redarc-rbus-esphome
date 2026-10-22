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


CONFIG_SCHEMA = (
    switch.switch_schema(RedarcRbusSwitch)
    .extend(
        {
            cv.GenerateID(CONF_REDARC_RBUS_ID): cv.use_id(RedarcRbus),
            cv.Required(CONF_ON_FRAME): _hex_frame,
            cv.Required(CONF_OFF_FRAME): _hex_frame,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_REDARC_RBUS_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_on_frame(config[CONF_ON_FRAME]))
    cg.add(var.set_off_frame(config[CONF_OFF_FRAME]))
