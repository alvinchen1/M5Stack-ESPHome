import esphome.codegen as cg
from esphome.components import i2c

# Manually declare Camera class
camera_ns = cg.esphome_ns.namespace("camera")
Camera = camera_ns.class_("Camera")

mlx90640_ns = cg.esphome_ns.namespace("mlx90640")
MLX90640Component = mlx90640_ns.class_("MLX90640Component", cg.PollingComponent, i2c.I2CDevice)
MLX90640Camera = mlx90640_ns.class_("MLX90640Camera", Camera)

CONF_MLX90640_ID = "mlx90640_id"
CONF_EMISSIVITY = "emissivity"
