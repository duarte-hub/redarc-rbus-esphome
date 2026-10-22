import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from . import RedarcRbus, redarc_rbus_ns

CODEOWNERS = ["@duarte-hub"]

CONF_REDARC_RBUS_ID = "redarc_rbus_id"

RedarcRbusSelect = redarc_rbus_ns.class_("RedarcRbusSelect", select.Select, cg.Component)

BROADCAST = 0xFF

# Settings the RedVision *user* app itself changes, written the same way (RBUS Parameter Set,
# index 0). Option index == raw value. Each: spn, destination (pgn whose sender owns the setting,
# fallback address), read-back (pgn, shift, bits), options, icon.
SETTINGS = {
    # OperatingModeSideDrawer.onOperatingModeTap: SPN 67 broadcast.
    "charging_mode": dict(
        spn=67, dest=(0, BROADCAST), readback=(127488, 0, 4),
        options=["Touring", "Storage"], icon="mdi:caravan",
    ),
    # ChargerOptionsWidget._onSaveCharger: SPN 104 to the charger node.
    "dc_input_trigger": dict(
        spn=104, dest=(127494, 0x01), readback=(127494, 56, 8),
        options=["Automatic", "12V", "24V", "Ignition", "Solar", "Always On",
                 "Smart Alternator", "Automatic LV"],
        icon="mdi:car-cog",
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_REDARC_RBUS_ID): cv.use_id(RedarcRbus),
        **{
            cv.Optional(key): select.select_schema(RedarcRbusSelect, icon=s["icon"]).extend(
                cv.COMPONENT_SCHEMA
            )
            for key, s in SETTINGS.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_REDARC_RBUS_ID])
    for key, s in SETTINGS.items():
        if key not in config:
            continue
        var = await select.new_select(config[key], options=s["options"])
        await cg.register_component(var, config[key])
        cg.add(var.set_parent(parent))
        cg.add(var.set_spn(s["spn"]))
        cg.add(var.set_destination(*s["dest"]))
        pgn, shift, bits = s["readback"]
        cg.add(parent.add_listener(var, pgn, shift, bits, 0, 0, 0))
