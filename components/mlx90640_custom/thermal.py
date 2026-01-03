print("DEBUG: START LOADING MLX90640_CUSTOM THERMAL.PY")
print(f"DEBUG: MODULE NAME: {__name__}")
__all__ = ["CONFIG_SCHEMA", "PLATFORM_SCHEMA", "to_code"]
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_NAME, CONF_ID

# Import shared definitions relative to this file
from . import ns

DEPENDENCIES = ["mlx90640_custom"]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ns.MLX90640Camera),
    cv.GenerateID(ns.CONF_MLX90640_ID): cv.use_id(ns.MLX90640Component),
    cv.Optional(CONF_NAME): cv.string,
})

PLATFORM_SCHEMA = CONFIG_SCHEMA

async def to_code(config):
    hub = await cg.get_variable(config[ns.CONF_MLX90640_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_parent(hub))
    
    await camera.register_camera(var, config)

print(f"DEBUG: END LOADING THERMAL_CAM_TEST CAMERA.PY")
print(f"DEBUG: HAS CONFIG_SCHEMA: {'CONFIG_SCHEMA' in globals()}")
print(f"DEBUG: HAS PLATFORM_SCHEMA: {'PLATFORM_SCHEMA' in globals()}")
print(f"DEBUG: CONFIG_SCHEMA TYPE: {type(CONFIG_SCHEMA)}")
print(f"DEBUG: ALL GLOBALS: {list(globals().keys())}")
