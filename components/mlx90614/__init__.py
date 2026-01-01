import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_UPDATE_INTERVAL,
    CONF_AMBIENT,
    CONF_OBJECT,
)

from esphome.components import i2c, sensor

CONF_DIAGNOSTIC_MODE = "diagnostic_mode"

mlx90614_ns = cg.esphome_ns.namespace("mlx90614")
MLX90614Component = mlx90614_ns.class_(
    "MLX90614Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MLX90614Component),
            cv.Required(CONF_ADDRESS): cv.i2c_address,
            cv.Optional(CONF_UPDATE_INTERVAL, default="5s"): cv.update_interval,
            cv.Optional(CONF_DIAGNOSTIC_MODE, default=False): cv.boolean,
            cv.Optional(CONF_AMBIENT): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional(CONF_OBJECT): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x5A))
    .extend(cv.polling_component_schema("5s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_diagnostic_mode(config[CONF_DIAGNOSTIC_MODE]))

    if CONF_AMBIENT in config:
        ambient = await sensor.new_sensor(config[CONF_AMBIENT])
        cg.add(var.set_ambient_sensor(ambient))

    if CONF_OBJECT in config:
        obj = await sensor.new_sensor(config[CONF_OBJECT])
        cg.add(var.set_object_sensor(obj))