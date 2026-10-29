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

from esphome.const import CONF_NAME

_LOGGER = logging.getLogger(__name__)

# Entity names (regex, case-insensitive, searched anywhere in the name) that stay on Zigbee.
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
# Entity kinds that are always exposed whatever their name (the control switches).
EXPOSE_ALL = {"validate_switch_esp32"}

# Never on Zigbee, even if they are switches (the Tailscale VPN toggle and friends).
_NEVER = re.compile(r"(^|\s)vpn\b", re.IGNORECASE)
_PATTERNS = [re.compile(p, re.IGNORECASE) for p in ZIGBEE_EXPOSE]
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
        if fn_name not in EXPOSE_ALL:
            if not any(p.search(name) for p in _PATTERNS):
                return config  # not registered with Zigbee; still a normal ESPHome entity
        return original(config, *args, **kwargs)

    setattr(zigbee, fn_name, filtered)


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
