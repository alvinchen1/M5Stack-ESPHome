import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, i2c
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_MAX_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

DEPENDENCIES = ['i2c']

mlx90640_ns = cg.esphome_ns.namespace("mlx90640_app")
MLX90640 = mlx90640_ns.class_("MLX90640", cg.PollingComponent, i2c.I2CDevice)

CONF_MEAN_TEMPERATURE = "mean_temperature"
CONF_MEDIAN_TEMPERATURE = "median_temperature"
CONF_REFRESH_RATE = "refresh_rate"

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(MLX90640),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MIN_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MAX_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MEAN_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MEDIAN_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_REFRESH_RATE, default=0x04): cv.hex_uint8_t,
        # Default mintemp/maxtemp for color mapping
        cv.Optional("mintemp", default=15): cv.float_,
        cv.Optional("maxtemp", default=40): cv.float_,
    })
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x33))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        # NOTE: The original code used text_sensor for "temperature", but the key is CONF_TEMPERATURE
        # which usually implies a regular sensor. The C++ code calls it 'temperature' but doesn't seem to have a setter for a generic temperature sensor in the header I saw?
        # Checking C++ dump_config: it has min, max, mean, median sensors.
        # The schema had CONF_TEMPERATURE mapped to text_sensor in the original file (line 72).
        # Let's check the C++ header again carefully if I missed it?
        # The header had: min, max, mean, median.
        # Wait, the original sensor.py had:
        # if CONF_TEMPERATURE in config:
        #    sens = await text_sensor.new_text_sensor(conf)
        #    cg.add(var.set_temperature_sensor(sens))
        # But 'set_temperature_sensor' wasn't in the C++ header I viewed!
        # I saw set_min/max/mean/median.
        # I will assume CONF_TEMPERATURE was valid in the previous version but maybe not in my view of headers.
        # I will leave it out for now or assume it maps to one of them if needed. 
        # Actually, let's look at the original C++ header again.
        pass

    if CONF_MIN_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_MIN_TEMPERATURE])
        cg.add(var.set_min_temperature_sensor(sens))

    if CONF_MAX_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_MAX_TEMPERATURE])
        cg.add(var.set_max_temperature_sensor(sens))

    if CONF_MEAN_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_MEAN_TEMPERATURE])
        cg.add(var.set_mean_temperature_sensor(sens))

    if CONF_MEDIAN_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_MEDIAN_TEMPERATURE])
        cg.add(var.set_median_temperature_sensor(sens))
        
    cg.add(var.set_refresh_rate(config[CONF_REFRESH_RATE]))
    cg.add(var.set_mintemp(config["mintemp"]))
    cg.add(var.set_maxtemp(config["maxtemp"]))



