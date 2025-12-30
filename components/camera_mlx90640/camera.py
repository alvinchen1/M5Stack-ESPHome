import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import camera

CONFIG_SCHEMA = camera.CAMERA_SCHEMA.extend({})
PLATFORM_SCHEMA = CONFIG_SCHEMA

async def to_code(config):
    pass
