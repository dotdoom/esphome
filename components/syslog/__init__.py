import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.const import CONF_ID, CONF_IP_ADDRESS, CONF_PORT, CONF_CLIENT_ID, CONF_LEVEL, CONF_PAYLOAD, CONF_TAG
from esphome.components.logger import LOG_LEVELS, is_log_level

CONF_STRIP_COLOR_CODES = "strip_color_codes"
CONF_FORWARD_LOGGER = "forward_logger"
CONF_MIN_LOG_LEVEL = "min_log_level"

DEPENDENCIES = ['logger', 'network']

syslog_ns = cg.esphome_ns.namespace('syslog')

Syslog = syslog_ns.class_('Syslog', cg.Component)
SyslogLogAction = syslog_ns.class_('SyslogLogAction', automation.Action)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Syslog),
    cv.Optional(CONF_IP_ADDRESS, default="255.255.255.255"): cv.string_strict,
    cv.Optional(CONF_PORT, default=514): cv.port,
    cv.Optional(CONF_FORWARD_LOGGER, default=True): cv.boolean,
    cv.Optional(CONF_STRIP_COLOR_CODES, default=True): cv.boolean,
    cv.Optional(CONF_MIN_LOG_LEVEL, default="DEBUG"): is_log_level,
    cv.Optional(CONF_CLIENT_ID): cv.string_strict,
})

SYSLOG_LOG_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(Syslog),
    cv.Required(CONF_LEVEL): cv.templatable(cv.int_range(min=0, max=7)),
    cv.Required(CONF_TAG): cv.templatable(cv.string),
    cv.Required(CONF_PAYLOAD): cv.templatable(cv.string),
})

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    cg.add(var.set_forward_logger(config[CONF_FORWARD_LOGGER]))
    cg.add(var.set_strip_color_codes(config[CONF_STRIP_COLOR_CODES]))
    cg.add(var.set_server_ip_address(config[CONF_IP_ADDRESS]))
    cg.add(var.set_server_port(config[CONF_PORT]))
    cg.add(var.set_min_log_level(LOG_LEVELS[config[CONF_MIN_LOG_LEVEL]]))
    if CONF_CLIENT_ID in config:
      cg.add(var.set_hostname(config[CONF_CLIENT_ID]))

@automation.register_action('syslog.log', SyslogLogAction, SYSLOG_LOG_ACTION_SCHEMA)
def syslog_log_action_to_code(config, action_id, template_arg, args):
    parent = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    template_ = yield cg.templatable(config[CONF_LEVEL], args, cg.int)
    cg.add(var.set_level(template_))
    template_ = yield cg.templatable(config[CONF_TAG], args, cg.std_string)
    cg.add(var.set_tag(template_))
    template_ = yield cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(template_))

    yield var
