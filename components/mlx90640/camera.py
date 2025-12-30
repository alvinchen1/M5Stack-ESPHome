import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera
from esphome.const import CONF_ID

# Removed DEPENDENCIES to avoid circular dependency loop with core component
# DEPENDENCIES = ["mlx90640"]

CONF_MLX90640_ID = "mlx90640_id"

# Re-define namespace locally to avoid circular import from __init__.py
mlx90640_ns = cg.esphome_ns.namespace("mlx90640_app")
MLX90640 = mlx90640_ns.class_("MLX90640", cg.PollingComponent)
MLX90640Camera = mlx90640_ns.class_("MLX90640Camera", camera.Camera)

CONFIG_SCHEMA = camera.CAMERA_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MLX90640Camera),
        cv.GenerateID(CONF_MLX90640_ID): cv.use_id(MLX90640),
    }
)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_MLX90640_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await cg.register_component(var, config)
    await camera.register_camera(var, config)
