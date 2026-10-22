import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import ble_client, sensor
from esphome.const import (
    CONF_ID,
    CONF_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from . import redarc_rbus_ns

CODEOWNERS = ["@duarte-hub"]
DEPENDENCIES = ["ble_client", "esp32_ble"]

CONF_STATE_OF_CHARGE = "state_of_charge"
CONF_PASSKEY = "passkey"
CONF_STATUS_LED = "status_led"

RedarcRbus = redarc_rbus_ns.class_(
    "RedarcRbus", cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RedarcRbus),
            # 6-digit passkey shown on the Display's Settings -> Bluetooth screen.
            cv.Optional(CONF_PASSKEY): cv.int_range(min=0, max=999999),
            # Optional activity LED, pulsed while BLE frames flow (e.g. onboard GPIO2).
            cv.Optional(CONF_STATUS_LED): pins.gpio_output_pin_schema,
            cv.Optional(CONF_STATE_OF_CHARGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if CONF_PASSKEY in config:
        cg.add(var.set_passkey(config[CONF_PASSKEY]))
    if CONF_STATUS_LED in config:
        pin = await cg.gpio_pin_expression(config[CONF_STATUS_LED])
        cg.add(var.set_status_led(pin))
    if CONF_STATE_OF_CHARGE in config:
        sens = await sensor.new_sensor(config[CONF_STATE_OF_CHARGE])
        cg.add(var.set_soc_sensor(sens))
    if CONF_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_VOLTAGE])
        cg.add(var.set_voltage_sensor(sens))
