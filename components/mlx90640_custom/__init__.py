print("DEBUG: LOADING MLX90640_CUSTOM __INIT__.PY")
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID
from esphome.core import CORE
from . import ns
DEPENDENCIES = ["i2c", "sensor"]
AUTO_LOAD = ["i2c", "sensor"]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ns.MLX90640Component),
    cv.Optional(ns.CONF_EMISSIVITY, default=0.95): cv.float_,
    # Allow specifying I2C bus pins and frequency in the component block
    cv.Optional("sda"): cv.int_,
    cv.Optional("scl"): cv.int_,
    cv.Optional("frequency"): cv.int_,
    cv.Optional("min_temperature"): sensor.sensor_schema(
        unit_of_measurement="°C", accuracy_decimals=1
    ),
    cv.Optional("max_temperature"): sensor.sensor_schema(
        unit_of_measurement="°C", accuracy_decimals=1
    ),
    cv.Optional("mean_temperature"): sensor.sensor_schema(
        unit_of_measurement="°C", accuracy_decimals=1
    ),
    cv.Optional("median_temperature"): sensor.sensor_schema(
        unit_of_measurement="°C", accuracy_decimals=1
    ),
    cv.Optional("mintemp", default=15.0): cv.float_,
    cv.Optional("maxtemp", default=40.0): cv.float_,
    cv.Optional("refresh_rate"): cv.int_,
}).extend(cv.polling_component_schema("60s")).extend(i2c.i2c_device_schema(0x33))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Enable Web Server support in C++
    cg.add_define("USE_MLX90640_WEB_SERVER")
    cg.add_global(cg.RawStatement('#include <esp_http_server.h>'))

    cg.add(var.set_emissivity(config[ns.CONF_EMISSIVITY]))
    
    if "min_temperature" in config:
        sens = await sensor.new_sensor(config["min_temperature"])
        cg.add(var.set_min_temperature_sensor(sens))
    
    if "max_temperature" in config:
        sens = await sensor.new_sensor(config["max_temperature"])
        cg.add(var.set_max_temperature_sensor(sens))
        
    if "mean_temperature" in config:
        sens = await sensor.new_sensor(config["mean_temperature"])
        cg.add(var.set_mean_temperature_sensor(sens))

    if "median_temperature" in config:
        sens = await sensor.new_sensor(config["median_temperature"])
        cg.add(var.set_median_temperature_sensor(sens))

    cg.add(var.set_min_image_temp(config["mintemp"]))
    cg.add(var.set_max_image_temp(config["maxtemp"]))



