import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_UPDATE_INTERVAL,
)

AUTO_LOAD = ["i2c"]

mlx90614_ns = cg.esphome_ns.namespace("mlx90614")
MLX90614Component = mlx90614_ns.class_(
    "MLX90614Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_AMBIENT_TEMPERATURE = "ambient_temperature"
CONF_OBJECT_TEMPERATURE = "object_temperature"
CONF_DIAGNOSTIC_MODE = "diagnostic_mode"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MLX90614Component),
            cv.Optional(CONF_AMBIENT_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional(CONF_OBJECT_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional(CONF_DIAGNOSTIC_MODE, default=False): cv.boolean,
            cv.Optional(CONF_ADDRESS, default=0x5A): cv.i2c_address,
            cv.Optional(CONF_UPDATE_INTERVAL, default="5s"): cv.update_interval,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x5A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # FIX: use set_i2c_address(), not set_address()
    cg.add(var.set_i2c_address(config[CONF_ADDRESS]))

    if CONF_AMBIENT_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_AMBIENT_TEMPERATURE])
        cg.add(var.set_ambient_sensor(sens))

    if CONF_OBJECT_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OBJECT_TEMPERATURE])
        cg.add(var.set_object_sensor(sens))

    cg.add(var.set_diagnostic_mode(config[CONF_DIAGNOSTIC_MODE]))