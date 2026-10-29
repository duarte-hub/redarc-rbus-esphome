import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

DEPENDENCIES = ["zigbee"]

CONF_RSSI = "rssi"
CONF_LQI = "lqi"

zigbee_link_ns = cg.esphome_ns.namespace("zigbee_link")
ZigbeeLink = zigbee_link_ns.class_("ZigbeeLink", cg.PollingComponent)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZigbeeLink),
            # Signal strength of the link to the parent (the coordinator or router we joined through).
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            # Link quality indicator, 0-255.
            cv.Optional(CONF_LQI): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon="mdi:signal",
            ),
        }
    )
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_RSSI in config:
        cg.add(var.set_rssi_sensor(await sensor.new_sensor(config[CONF_RSSI])))
    if CONF_LQI in config:
        cg.add(var.set_lqi_sensor(await sensor.new_sensor(config[CONF_LQI])))
