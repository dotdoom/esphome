import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, remote_transmitter
from esphome.const import CONF_ID

DEPENDENCIES = ["mqtt", "remote_transmitter"]
AUTO_LOAD = ["mqtt"]

somfy_ns = cg.esphome_ns.namespace("somfy")
SomfyRTSCover = somfy_ns.class_("SomfyRTSCover", cover.Cover, cg.Component)

CONF_TRANSMITTER = "transmitter"
CONF_REMOTE_ID_BASE = "remote_id_base"
CONF_REMOTE_ID_OFFSET = "remote_id_offset"

CONFIG_SCHEMA = cover.cover_schema(SomfyRTSCover).extend(
    {
        cv.Required(CONF_TRANSMITTER): cv.use_id(remote_transmitter.RemoteTransmitterComponent),
        cv.Required(CONF_REMOTE_ID_BASE): cv.uint32_t,
        cv.Required(CONF_REMOTE_ID_OFFSET): cv.uint32_t,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER])
    cg.add(var.set_transmitter(transmitter))
    cg.add(var.set_remote_id(
        config[CONF_REMOTE_ID_BASE] + config[CONF_REMOTE_ID_OFFSET]
    ))
