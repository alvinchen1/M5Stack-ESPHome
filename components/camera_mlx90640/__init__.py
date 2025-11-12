import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, i2c
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_FREQUENCY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

# Import web server if available
try:
    from esphome.components import web_server_base
    CONF_WEB_SERVER_ID = "web_server_id"
    USE_WEBSERVER = True
except ImportError:
    USE_WEBSERVER = False

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]

# Component namespace
mlx90640_ns = cg.esphome_ns.namespace("mlx90640_app")
MLX90640 = mlx90640_ns.class_(
    "MLX90640",
    cg.PollingComponent,  # Keep as PollingComponent
    i2c.I2CDevice
)

# Configuration keys
CONF_SDA = "sda"
CONF_SCL = "scl"
CONF_MINTEMP = "mintemp"
CONF_MAXTEMP = "maxtemp"
CONF_REFRESH_RATE = "refresh_rate"
CONF_MIN_TEMPERATURE = "min_temperature"
CONF_MAX_TEMPERATURE = "max_temperature"
CONF_MEAN_TEMPERATURE = "mean_temperature"
CONF_MEDIAN_TEMPERATURE = "median_temperature"

# Configuration schema
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MLX90640),
            cv.Required(CONF_SDA): cv.int_,
            cv.Required(CONF_SCL): cv.int_,
            cv.Optional(CONF_FREQUENCY, default=400000): cv.int_,
            cv.Optional(CONF_ADDRESS, default=0x33): cv.i2c_address,
            cv.Optional(CONF_MINTEMP, default=0): cv.float_,
            cv.Optional(CONF_MAXTEMP, default=80): cv.float_,
            cv.Optional(CONF_REFRESH_RATE, default=0x04): cv.hex_uint8_t,
            cv.Optional(CONF_MIN_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MAX_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MEAN_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MEDIAN_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(default_address=0x33))
)

# Add web server support (optional, backward compatible)
if USE_WEBSERVER:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        {
            cv.Optional(CONF_WEB_SERVER_ID): cv.use_id(web_server_base.WebServerBase),
        }
    )


async def to_code(config):
    # Create the MLX90640 component
    var = cg.new_Pvariable(config[CONF_ID])
    
    # Register as polling component
    await cg.register_component(var, config)
    
    # Register as I2C device
    await i2c.register_i2c_device(var, config)

    # Set pin configuration
    cg.add(var.set_sda_pin(config[CONF_SDA]))
    cg.add(var.set_scl_pin(config[CONF_SCL]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_mintemp(config[CONF_MINTEMP]))
    cg.add(var.set_maxtemp(config[CONF_MAXTEMP]))
    cg.add(var.set_refresh_rate(config[CONF_REFRESH_RATE]))

    # Configure temperature sensors if present
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

    # Web server support (optional, backward compatible)
    if USE_WEBSERVER and CONF_WEB_SERVER_ID in config:
        web_server = await cg.get_variable(config[CONF_WEB_SERVER_ID])
        cg.add(var.set_base(web_server))
        cg.add_define("USE_WEBSERVER")
