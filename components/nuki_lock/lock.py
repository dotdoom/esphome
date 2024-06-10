import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.const as c
from esphome.components import lock, binary_sensor, text_sensor, sensor, button, time

AUTO_LOAD = ["binary_sensor", "text_sensor", "sensor", "button"]

CONF_PAIRED = "paired"
CONF_ERROR = "error"
CONF_BATTERY_CRITICAL = "battery_critical"
CONF_DOOR_CONTACT = "door_contact"
CONF_DOOR_CONTACT_SENSOR = "door_contact_sensor"
CONF_BEACON_RSSI = "beacon_rssi"
CONF_BEACON_LATENCY = "beacon_latency"
CONF_HEARTBEAT_LATENCY = "heartbeat_latency"
CONF_BEACON_BLE_ADDRESS = "beacon_ble_address"
CONF_LOCK_CURRENT_DATETIME = "lock_current_datetime"
CONF_TRIGGER = "trigger"
CONF_CONFIG_UPDATE_COUNT = "config_update_count"
CONF_LAST_ACTION = "last_action"
CONF_LAST_ACTION_TRIGGER = "last_action_trigger"
CONF_LAST_ACTION_COMPLETION_STATUS = "last_action_completion_status"
CONF_NIGHT_MODE_ACTIVE = "night_mode_active"
CONF_BATTERY_RESISTANCE = "battery_resistance"
CONF_BATTERY_LOWEST_VOLTAGE = "battery_lowest_voltage"
CONF_LAST_ACTION_START_VOLTAGE = "last_action_start_voltage"
CONF_LAST_ACTION_LOCK_DISTANCE = "last_action_lock_distance"
CONF_LAST_ACTION_START_TEMPERATURE = "last_action_start_temperature"
CONF_LAST_ACTION_BATTERY_DRAIN = "last_action_battery_drain"
CONF_LAST_ACTION_MAX_TURN_CURRENT = "last_action_max_turn_current"
CONF_REQUEST_BATTERY_REPORTS = "request_battery_reports"
CONF_RESTART_AFTER_BEACON_LATENCY = "restart_after_beacon_latency"
CONF_REQUEST_STATE = "request_state"
CONF_UNPAIR = "unpair"
CONF_TIME_SOURCE = "time_source"

nuki_lock_ns = cg.esphome_ns.namespace("nuki_lock")
NukiLockComponent = nuki_lock_ns.class_("NukiLockComponent", lock.Lock, cg.PollingComponent)
RequestStateButton = nuki_lock_ns.class_("RequestStateButton", button.Button)
UnpairButton = nuki_lock_ns.class_("UnpairButton", button.Button)

CONFIG_SCHEMA = lock.LOCK_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(NukiLockComponent),

    # Required sensors (if unconfigured, defaults are used).
    cv.Optional(CONF_PAIRED, default={
        "name": "Paired",
    }): binary_sensor.binary_sensor_schema(
        device_class=c.DEVICE_CLASS_CONNECTIVITY,
    ),
    cv.Optional(CONF_ERROR, default={
        "name": "Error",
    }): text_sensor.text_sensor_schema(
        icon="mdi:alert",
    ),
    cv.Optional(CONF_BATTERY_CRITICAL, default={
        "name": "Battery critical",
    }): binary_sensor.binary_sensor_schema(
        device_class=c.DEVICE_CLASS_BATTERY,
    ),
    cv.Optional(c.CONF_BATTERY_LEVEL, default={
        "name": "Battery",
    }): sensor.sensor_schema(
        device_class=c.DEVICE_CLASS_BATTERY,
        unit_of_measurement=c.UNIT_PERCENT,
    ),
    cv.Optional(CONF_DOOR_CONTACT, default={
        "name": "Door contact",
    }): binary_sensor.binary_sensor_schema(
        device_class=c.DEVICE_CLASS_DOOR,
    ),
    cv.Optional(CONF_DOOR_CONTACT_SENSOR, default={
        "name": "Door contact sensor",
    }): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_TRIGGER, default={
        "name": "Trigger",
    }): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_NIGHT_MODE_ACTIVE, default={
        "name": "Night mode active",
    }): binary_sensor.binary_sensor_schema(
        icon="mdi:weather-night",
    ),

    # Optional sensors.
    cv.Optional(CONF_BEACON_RSSI): sensor.sensor_schema(
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=c.STATE_CLASS_MEASUREMENT,
        device_class=c.DEVICE_CLASS_SIGNAL_STRENGTH,
        unit_of_measurement=c.UNIT_DECIBEL_MILLIWATT,
    ),
    cv.Optional(CONF_BEACON_LATENCY): sensor.sensor_schema(
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=c.STATE_CLASS_MEASUREMENT,
        unit_of_measurement=c.UNIT_MILLISECOND,
    ),
    cv.Optional(CONF_HEARTBEAT_LATENCY): sensor.sensor_schema(
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=c.STATE_CLASS_MEASUREMENT,
        unit_of_measurement=c.UNIT_MILLISECOND,
    ),
    cv.Optional(CONF_BEACON_BLE_ADDRESS): text_sensor.text_sensor_schema(
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:bluetooth",
    ),
    cv.Optional(CONF_LOCK_CURRENT_DATETIME): text_sensor.text_sensor_schema(
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=c.DEVICE_CLASS_TIMESTAMP,
    ),
    cv.Optional(CONF_CONFIG_UPDATE_COUNT): sensor.sensor_schema(
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        state_class=c.STATE_CLASS_TOTAL,
    ),
    cv.Optional(CONF_LAST_ACTION): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_LAST_ACTION_TRIGGER): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_LAST_ACTION_COMPLETION_STATUS): text_sensor.text_sensor_schema(),

    # Optional sensors - battery report.
    cv.Optional(CONF_REQUEST_BATTERY_REPORTS, default=False): cv.boolean,
    cv.Optional(c.CONF_BATTERY_VOLTAGE, default={
        "name": "Battery voltage",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=c.DEVICE_CLASS_VOLTAGE,
        unit_of_measurement=c.UNIT_VOLT,
        accuracy_decimals=3,
    ),
    cv.Optional(CONF_BATTERY_RESISTANCE, default={
        "name": "Battery resistance",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        unit_of_measurement=c.UNIT_OHM,
        accuracy_decimals=3,
        icon="mdi:resistor",
    ),
    cv.Optional(CONF_BATTERY_LOWEST_VOLTAGE, default={
        "name": "Battery lowest voltage",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=c.DEVICE_CLASS_VOLTAGE,
        unit_of_measurement=c.UNIT_VOLT,
        accuracy_decimals=3,
    ),
    cv.Optional(CONF_LAST_ACTION_START_VOLTAGE, default={
        "name": "Last action start voltage",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=c.DEVICE_CLASS_VOLTAGE,
        unit_of_measurement=c.UNIT_VOLT,
        accuracy_decimals=3,
    ),
    cv.Optional(CONF_LAST_ACTION_LOCK_DISTANCE, default={
        "name": "Last action lock distance",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        unit_of_measurement=c.UNIT_DEGREES,
        icon="mdi:rotate-360",
    ),
    cv.Optional(CONF_LAST_ACTION_START_TEMPERATURE, default={
        "name": "Last action start temperature",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        unit_of_measurement=c.UNIT_DEGREES,
        device_class=c.DEVICE_CLASS_TEMPERATURE,
    ),
    cv.Optional(CONF_LAST_ACTION_BATTERY_DRAIN, default={
        "name": "Last action battery drain",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        unit_of_measurement=c.UNIT_WATT_HOURS,
        device_class=c.DEVICE_CLASS_ENERGY,
        accuracy_decimals=3,
    ),
    cv.Optional(CONF_LAST_ACTION_MAX_TURN_CURRENT, default={
        "name": "Last action max turn current",
    }): sensor.sensor_schema(
        state_class=c.STATE_CLASS_MEASUREMENT,
        entity_category=c.ENTITY_CATEGORY_DIAGNOSTIC,
        unit_of_measurement=c.UNIT_AMPERE,
        device_class=c.DEVICE_CLASS_CURRENT,
        accuracy_decimals=3,
        # DEFAULT DISABLE. A few others, too.
    ),

    # Configuration.
    cv.Optional(CONF_RESTART_AFTER_BEACON_LATENCY, default="10min"): cv.positive_time_period_milliseconds,

    # Actions.
    cv.Optional(CONF_REQUEST_STATE, default={
        "name": "Request state",
    }): button.button_schema(RequestStateButton,
        entity_category=c.ENTITY_CATEGORY_CONFIG,
        icon="mdi:refresh",
    ),
    cv.Optional(CONF_UNPAIR, default={
        "name": "Unpair",
    }): button.button_schema(
        UnpairButton,
        entity_category=c.ENTITY_CATEGORY_CONFIG,
        icon="mdi:close",
    ),

    # Time autoupdate.
    cv.Optional(c.CONF_PIN): cv.positive_int,
    cv.Optional(CONF_TIME_SOURCE): cv.use_id(time.RealTimeClock),
}).extend(cv.polling_component_schema("30min"))

async def to_code(config):
    var = cg.new_Pvariable(config[c.CONF_ID])
    await cg.register_component(var, config)
    await lock.register_lock(var, config)

    request_state_button = await button.new_button(config[CONF_REQUEST_STATE])
    await cg.register_parented(request_state_button, config[c.CONF_ID])
    cg.add(var.set_request_state_button(request_state_button))

    unpair_button = await button.new_button(config[CONF_UNPAIR])
    await cg.register_parented(unpair_button, config[c.CONF_ID])
    cg.add(var.set_request_state_button(unpair_button))

    cg.add(var.set_paired_binary_sensor(await binary_sensor.new_binary_sensor(config[CONF_PAIRED])))
    cg.add(var.set_error_text_sensor(await text_sensor.new_text_sensor(config[CONF_ERROR])))
    cg.add(var.set_battery_critical_binary_sensor(await binary_sensor.new_binary_sensor(config[CONF_BATTERY_CRITICAL])))
    cg.add(var.set_battery_level_sensor(await sensor.new_sensor(config[c.CONF_BATTERY_LEVEL])))
    cg.add(var.set_door_contact_binary_sensor(await binary_sensor.new_binary_sensor(config[CONF_DOOR_CONTACT])))
    cg.add(var.set_door_contact_sensor_text_sensor(await text_sensor.new_text_sensor(config[CONF_DOOR_CONTACT_SENSOR])))
    cg.add(var.set_trigger_text_sensor(await text_sensor.new_text_sensor(config[CONF_TRIGGER])))
    cg.add(var.set_night_mode_active_binary_sensor(await binary_sensor.new_binary_sensor(config[CONF_NIGHT_MODE_ACTIVE])))

    if CONF_BEACON_RSSI in config:
        cg.add(var.set_beacon_rssi_sensor( await sensor.new_sensor(config[CONF_BEACON_RSSI])))

    if CONF_BEACON_LATENCY in config:
        cg.add(var.set_beacon_latency_sensor(await sensor.new_sensor(config[CONF_BEACON_LATENCY])))

    if CONF_HEARTBEAT_LATENCY in config:
        cg.add(var.set_heartbeat_latency_sensor(await sensor.new_sensor(config[CONF_HEARTBEAT_LATENCY])))

    if CONF_BEACON_BLE_ADDRESS in config:
        cg.add(var.set_beacon_ble_address_text_sensor(await text_sensor.new_text_sensor(config[CONF_BEACON_BLE_ADDRESS])))

    if CONF_LOCK_CURRENT_DATETIME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LOCK_CURRENT_DATETIME])
        cg.add(var.set_lock_current_datetime_text_sensor(sens))

    if CONF_CONFIG_UPDATE_COUNT in config:
        cg.add(var.set_config_update_count_sensor(await sensor.new_sensor(config[CONF_CONFIG_UPDATE_COUNT])))

    if CONF_LAST_ACTION in config:
        cg.add(var.set_last_action_text_sensor(await text_sensor.new_text_sensor(config[CONF_LAST_ACTION])))

    if CONF_LAST_ACTION_TRIGGER in config:
        cg.add(var.set_last_action_trigger_text_sensor(await text_sensor.new_text_sensor(config[CONF_LAST_ACTION_TRIGGER])))

    if CONF_LAST_ACTION_COMPLETION_STATUS in config:
        cg.add(var.set_last_action_completion_status_text_sensor(await text_sensor.new_text_sensor(config[CONF_LAST_ACTION_COMPLETION_STATUS])))

    if config.get(CONF_REQUEST_BATTERY_REPORTS):
        cg.add(var.set_request_battery_reports(True))
        cg.add(var.set_battery_voltage_sensor(await sensor.new_sensor(config[c.CONF_BATTERY_VOLTAGE])))
        cg.add(var.set_battery_resistance_sensor(await sensor.new_sensor(config[CONF_BATTERY_RESISTANCE])))
        cg.add(var.set_battery_lowest_voltage_sensor(await sensor.new_sensor(config[CONF_BATTERY_LOWEST_VOLTAGE])))
        cg.add(var.set_last_action_start_voltage_sensor(await sensor.new_sensor(config[CONF_LAST_ACTION_START_VOLTAGE])))
        cg.add(var.set_last_action_lock_distance_sensor(await sensor.new_sensor(config[CONF_LAST_ACTION_LOCK_DISTANCE])))
        cg.add(var.set_last_action_start_temperature_sensor(await sensor.new_sensor(config[CONF_LAST_ACTION_START_TEMPERATURE])))
        cg.add(var.set_last_action_battery_drain_sensor(await sensor.new_sensor(config[CONF_LAST_ACTION_BATTERY_DRAIN])))
        cg.add(var.set_last_action_max_turn_current_sensor(await sensor.new_sensor(config[CONF_LAST_ACTION_MAX_TURN_CURRENT])))

    if CONF_RESTART_AFTER_BEACON_LATENCY in config:
        cg.add(var.set_restart_after_beacon_latency(config[CONF_RESTART_AFTER_BEACON_LATENCY]))

    if c.CONF_PIN in config:
        cg.add(var.set_pin(config[c.CONF_PIN]))

    if CONF_TIME_SOURCE in config:
        cg.add(var.set_time_source(await cg.get_variable(config[CONF_TIME_SOURCE])))
