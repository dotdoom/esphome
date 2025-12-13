import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.const as c
from esphome.components import climate, sensor, remote_transmitter

DEPENDENCIES = ["climate"]
AUTO_LOAD = ["sensor"]
CONF_TRANSMITTER_ID = "transmitter_id"
CONF_TEMPERATURE_SENSOR_ID = "temperature_sensor_id"

zehnder_ns = cg.esphome_ns.namespace("zehnder")
ZehnderComponent = zehnder_ns.class_("ZehnderComponent", climate.Climate, cg.PollingComponent)

CONFIG_SCHEMA = climate.climate_schema(ZehnderComponent).extend({
    cv.Required(CONF_TRANSMITTER_ID): cv.use_id(remote_transmitter.RemoteTransmitterComponent),
    cv.Optional(CONF_TEMPERATURE_SENSOR_ID): cv.use_id(sensor.Sensor),
}).extend(cv.polling_component_schema("1min"))

async def to_code(config):
    var = cg.new_Pvariable(config[c.CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))

    if CONF_TEMPERATURE_SENSOR_ID in config:
        temperature_sensor = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR_ID])
        cg.add(var.add_temperature_sensor(temperature_sensor))
