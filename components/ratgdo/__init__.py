from dataclasses import dataclass

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.core import CORE
from esphome.coroutine import CoroPriority, coroutine_with_priority
import voluptuous as vol

DEPENDENCIES = ["preferences"]
MULTI_CONF = False

DOMAIN = "ratgdo"

ratgdo_ns = cg.esphome_ns.namespace("ratgdo")
RATGDO = ratgdo_ns.class_("RATGDOComponent", cg.Component)


@dataclass
class RATGDOData:
    """Track observable subscriber counts for compile-time sizing."""

    door_state: int = 0
    door_action_delayed: int = 0
    distance: int = 0
    vehicle_detected: int = 0
    vehicle_arriving: int = 0
    vehicle_leaving: int = 0


def _get_data() -> RATGDOData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = RATGDOData()
    return CORE.data[DOMAIN]


def subscribe_door_state() -> None:
    _get_data().door_state += 1


def subscribe_door_action_delayed() -> None:
    _get_data().door_action_delayed += 1


def subscribe_distance() -> None:
    _get_data().distance += 1


def subscribe_vehicle_detected() -> None:
    _get_data().vehicle_detected += 1


def subscribe_vehicle_arriving() -> None:
    _get_data().vehicle_arriving += 1


def subscribe_vehicle_leaving() -> None:
    _get_data().vehicle_leaving += 1


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_subscriber_defines():
    """Emit observable subscriber count defines after all children have registered."""
    data = _get_data()
    cg.add_define("RATGDO_MAX_DOOR_STATE_SUBSCRIBERS", data.door_state)
    cg.add_define(
        "RATGDO_MAX_DOOR_ACTION_DELAYED_SUBSCRIBERS", data.door_action_delayed
    )
    cg.add_define("RATGDO_MAX_DISTANCE_SUBSCRIBERS", data.distance)
    cg.add_define("RATGDO_MAX_VEHICLE_DETECTED_SUBSCRIBERS", data.vehicle_detected)
    cg.add_define("RATGDO_MAX_VEHICLE_ARRIVING_SUBSCRIBERS", data.vehicle_arriving)
    cg.add_define("RATGDO_MAX_VEHICLE_LEAVING_SUBSCRIBERS", data.vehicle_leaving)


SyncFailed = ratgdo_ns.class_("SyncFailed", automation.Trigger.template())

CONF_OUTPUT_GDO = "output_gdo_pin"
DEFAULT_OUTPUT_GDO = (
    "D4"  # D4 red control terminal / GarageDoorOpener (UART1 TX) pin is D4 on D1 Mini
)
CONF_INPUT_GDO = "input_gdo_pin"
DEFAULT_INPUT_GDO = (
    "D2"  # D2 red control terminal / GarageDoorOpener (UART1 RX) pin is D2 on D1 Mini
)

CONF_RATGDO_ID = "ratgdo_id"

CONF_ON_SYNC_FAILED = "on_sync_failed"

CONF_PROTOCOL = "protocol"

PROTOCOL_SECPLUSV2 = "secplusv2"
SUPPORTED_PROTOCOLS = [PROTOCOL_SECPLUSV2]


def validate_protocol(config):
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RATGDO),
            cv.Optional(
                CONF_OUTPUT_GDO, default=DEFAULT_OUTPUT_GDO
            ): pins.gpio_output_pin_schema,
            cv.Optional(
                CONF_INPUT_GDO, default=DEFAULT_INPUT_GDO
            ): pins.gpio_input_pin_schema,
            cv.Optional(CONF_ON_SYNC_FAILED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SyncFailed),
                }
            ),
            cv.Optional(CONF_PROTOCOL, default=PROTOCOL_SECPLUSV2): cv.All(
                vol.In(SUPPORTED_PROTOCOLS)
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_protocol,
)

RATGDO_CLIENT_SCHMEA = cv.Schema(
    {
        cv.GenerateID(CONF_RATGDO_ID): cv.use_id(RATGDO),
    }
)


async def register_ratgdo_child(var, config):
    parent = await cg.get_variable(config[CONF_RATGDO_ID])
    cg.add(var.set_parent(parent))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_OUTPUT_GDO])
    cg.add(var.set_output_gdo_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_INPUT_GDO])
    cg.add(var.set_input_gdo_pin(pin))

    for conf in config.get(CONF_ON_SYNC_FAILED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if CORE.is_esp32 and not CORE.using_arduino:
        from esphome.components import esp32

        esp32.include_builtin_idf_component("esp_driver_rmt")
        esp32.add_idf_component(
            name="secplus",
            repo="https://github.com/ratgdo/secplus.git",
            ref="add-esp-idf-support",
        )
    else:
        cg.add_library(
            name="secplus",
            repository="https://github.com/ratgdo/secplus#f98c3220356c27717a25102c0b35815ebbd26ccc",
            version=None,
        )
    if CORE.is_esp8266:
        cg.add_library(
            name="espsoftwareserial",
            repository="https://github.com/ratgdo/espsoftwareserial#autobaud",
            version=None,
        )

    cg.add_define("PROTOCOL_SECPLUSV2")
    cg.add(var.init_protocol())

    # RATGDOComponent::setup() subscribes to door_state
    subscribe_door_state()

    # Emit observable subscriber count defines after all children register
    CORE.add_job(_emit_subscriber_defines)
