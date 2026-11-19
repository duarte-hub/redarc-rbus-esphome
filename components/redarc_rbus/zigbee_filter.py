"""Expose only a short list of entities over Zigbee.

ESPHome's `zigbee:` component exposes every named sensor, binary sensor, switch and number,
and the only way to opt an entity out is `internal: true`, which also hides it from Home
Assistant and the web page. This hides entities from *Zigbee only*: on ESP32 the exposed
entities are collected while the config is validated, so the `validate_*_esp32` functions
are wrapped and skip anything that is not on the list. Edit ZIGBEE_EXPOSE to change it.

It does nothing unless `zigbee:` is in the config, and if a future ESPHome renames these
functions it leaves them alone (every entity is then exposed, as without this file).
"""

import logging
import re

import esphome.codegen as cg
from esphome.const import CONF_ACCURACY_DECIMALS, CONF_ID, CONF_NAME, CONF_REPORT

_LOGGER = logging.getLogger(__name__)

# Entity names (regex, case-insensitive, searched anywhere in the name) that stay on Zigbee.
# The first release's list. These keep the endpoints they had then: numbered in config order
# from 1 per kind (analog inputs, binary inputs, switches), as ESPHome did on its own.
ZIGBEE_EXPOSE = [
    r"battery so[ch]$",
    r"battery voltage$",
    r"battery current$",
    r"battery power$",
    r"battery temperature$",
    r"time remaining$",
    r"solar power$",
    r"tank \d level$",
    r"fault active$",
    r"wifi (signal|rssi)",
    r"ble (signal|rssi)",
]
# Added later. ESPHome numbers unpinned endpoints in config order, so adding one in the middle
# shifts every later endpoint and Zigbee2MQTT / ZHA then show the wrong reading on existing
# entities. These are numbered after the original ones instead (from NEW_BASE, in config order).
ZIGBEE_EXPOSE_ADDED = [
    r"solar (voltage|current)$",
    r"(ac|dc|solar) input active$",
    r"(ac|dc) input (voltage|current)$",
    r"charger output current$",
    r"load current$",
    r"inverter (on|power)$",
]
NEW_BASE = {"validate_sensor_esp32": 12, "validate_binary_sensor_esp32": 2}
# Added for the RedVision dashboard over Zigbee: settings, alarms and the text values as codes
# (Zigbee carries numbers, not text). Numbered after the "added" ones, from NEW_BASE_2.
ZIGBEE_EXPOSE_ADDED_2 = [
    r"state of health$",
    r"charger output voltage$",
    r"inverter output voltage$",
    r"battery capacity$",
    r"battery max charge current$",
    r"charger max output current$",
    r"alarm set point$",
    r"load disconnect (voltage|soc) (off|on)$",
    r"solar energy (today|yesterday)$",
    r"soc (min|max) \(last day\)$",
    r"voltage input \d$",
    r" code$",  # Zigbee-only template sensors (see common.yaml)
    r"charger derating$",
    r"low battery (soc|voltage) alarm$",
    r"load disconnect active$",
    r"ignition$",
    r"tank \d alarm$",
    r"pairing code$",  # numbers
    r" setting$",
]
NEW_BASE_2 = {"validate_sensor_esp32": 21, "validate_binary_sensor_esp32": 6, "validate_number_esp32": 1}
# Entity kinds that are always exposed whatever their name (the control switches).
EXPOSE_ALL = {"validate_switch_esp32"}

# Never on Zigbee, even if they are switches (the Tailscale VPN toggle and friends).
_NEVER = re.compile(r"(^|\s)vpn\b", re.IGNORECASE)
_PATTERNS = [re.compile(p, re.IGNORECASE) for p in ZIGBEE_EXPOSE]
_ADDED = [re.compile(p, re.IGNORECASE) for p in ZIGBEE_EXPOSE_ADDED]
_ADDED_2 = [re.compile(p, re.IGNORECASE) for p in ZIGBEE_EXPOSE_ADDED_2]
_BASES = {"orig": {}, "added": NEW_BASE, "added2": NEW_BASE_2}
_installed = False


def _wrap(zigbee, fn_name):
    original = getattr(zigbee, fn_name, None)
    if original is None:
        _LOGGER.warning("zigbee.%s not found; exposing every entity of that kind", fn_name)
        return

    def filtered(config, *args, **kwargs):
        name = config.get(CONF_NAME) or ""
        if _NEVER.search(name):
            return config
        group = "orig"
        if fn_name not in EXPOSE_ALL:
            if any(p.search(name) for p in _PATTERNS):
                group = "orig"
            elif any(p.search(name) for p in _ADDED):
                group = "added"
            elif any(p.search(name) for p in _ADDED_2):
                group = "added2"
            else:
                return config  # not registered with Zigbee; still a normal ESPHome entity
        if config.get(CONF_REPORT) is None:
            # Report straight to the coordinator when zigbee_sync asks (no binding needed, see zigbee_sync.h)
            config[CONF_REPORT] = _report_enable()
        if config.get("endpoint") is None:
            # Per-run counters ((kind, group) -> next endpoint); CORE.data is reset every run.
            from esphome.core import CORE
            nxt = CORE.data.setdefault("redarc_rbus_zigbee_ep", {})
            key = (fn_name, group)
            ep = nxt.get(key, _BASES[group].get(fn_name, 1))
            nxt[key] = ep + 1
            config["endpoint"] = ep
        config = original(config, *args, **kwargs)
        _record(fn_name, config)
        return config

    setattr(zigbee, fn_name, filtered)


# Exposed entities, for zigbee_sync: id(entity config) -> (kind, attribute id, entity id, accuracy decimals)
_KINDS = {
    "validate_sensor_esp32": "sensor",
    "validate_binary_sensor_esp32": "binary_sensor",
    "validate_switch_esp32": "switch",
    "validate_number_esp32": "number",
}


_SYNC_ID = "redarc_rbus_zigbee_sync"


def _report_enable():
    import esphome.config_validation as cv
    from esphome.core.entity_helpers import ZIGBEE_REPORT

    return cv.enum(ZIGBEE_REPORT, lower=True)("enable")


def _record(fn_name, config):
    kind = _KINDS.get(fn_name)
    if kind is None or CONF_ID not in config:
        return
    # The attribute ESPHome connects to the entity: presentValue (0x0055)
    for attr in config.get("zigbee_attributes", []):
        if attr.get("attribute_id") == 0x55 and attr.get("id") is not None:
            from esphome.core import CORE

            entries = CORE.data.setdefault("redarc_rbus_zigbee_sync", {})
            # Counted now: ESPHome sizes its component list from the IDs seen while validating
            CORE.component_ids.add(_SYNC_ID)
            entries[id(config)] = (kind, attr["id"], config[CONF_ID], config.get(CONF_ACCURACY_DECIMALS))
            return


async def sync_to_code():
    """Adds the zigbee_sync component for the exposed entities (no-op without `zigbee:`)."""
    from esphome.core import CORE

    entries = CORE.data.get("redarc_rbus_zigbee_sync", {})
    zb_conf = CORE.config.get("zigbee")
    if not entries or not zb_conf:
        return
    # A bigger stack for the Zigbee task (see zigbee_stack.cpp)
    cg.add_build_flag("-Wl,--wrap=xTaskCreatePinnedToCore")
    cg.add_build_flag("-Wl,--undefined=__wrap_xTaskCreatePinnedToCore")
    if isinstance(zb_conf, list):
        zb_conf = zb_conf[0]
    ns = cg.esphome_ns.namespace("redarc_rbus")
    sync_t = ns.class_("ZigbeeSync", cg.Component)
    zb = await cg.get_variable(zb_conf[CONF_ID])
    from esphome.core import ID

    var = cg.new_Pvariable(ID(_SYNC_ID, is_declaration=True, type=sync_t), zb)
    await cg.register_component(var, {})
    for kind, attr_id, entity_id, decimals in entries.values():
        attr = await cg.get_variable(attr_id)
        entity = await cg.get_variable(entity_id)
        if kind == "sensor":
            min_change = 10.0 ** -(decimals if decimals is not None else 0)
            cg.add(var.add_sensor(attr, entity, min_change))
        elif kind == "binary_sensor":
            cg.add(var.add_binary_sensor(attr, entity))
        elif kind == "number":
            cg.add(var.add_number(attr, entity))
        else:
            cg.add(var.add_switch(attr, entity))


def install():
    global _installed
    if _installed:
        return
    try:
        from esphome.components import zigbee
    except ImportError:
        return
    for fn_name in (
        "validate_sensor_esp32",
        "validate_binary_sensor_esp32",
        "validate_switch_esp32",
        "validate_number_esp32",
    ):
        _wrap(zigbee, fn_name)
    _installed = True
