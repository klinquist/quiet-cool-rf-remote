import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, fan, number, select, sensor, spi, switch, text_sensor
from esphome.const import (
    CONF_CLK_PIN,
    CONF_ID,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_NAME,
    CONF_OUTPUT_ID,
    CONF_SPEED_COUNT,
    DEVICE_CLASS_DURATION,
    STATE_CLASS_MEASUREMENT,
)
from esphome.core import CORE
from .. import quiet_cool_ns

CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_REMOTE_ID = "remote_id"
CONF_FREQ_MHZ = "center_freq_mhz"
CONF_DEVIATION_KHZ = "deviation_khz"
CONF_LEARN_BUTTON = "learn_button"
CONF_LEARNING_STATUS = "learning_status"
CONF_RX_PACKET_SENSOR = "rx_packet_sensor"
CONF_TEST_BUTTONS = "test_buttons"
CONF_REMOTE_LISTENER = "remote_listener"
CONF_ADDITIONAL_REMOTE_IDS = "additional_remote_ids"
CONF_TIMER = "timer"
CONF_TIMER_CONTROL = "control"
CONF_TIMER_REMAINING = "remaining"
CONF_STATUS_POLLING = "status_polling"
CONF_POLL_ENABLED = "enabled"
CONF_POLL_INTERVAL_SECONDS = "interval_seconds"
CONF_POLL_SWITCH = "switch"
CONF_POLL_INTERVAL = "interval"
CONF_COMMAND = "command"
CONF_FREQUENCY_CONTROLS = "frequency_controls"
CONF_CENTER_NUMBER = "center_freq_mhz"
CONF_DEVIATION_NUMBER = "deviation_khz"

QuietCoolFan = quiet_cool_ns.class_("QuietCoolFan", cg.Component, fan.Fan, spi.SPIDevice)
QuietCoolLearnButton = quiet_cool_ns.class_("QuietCoolLearnButton", button.Button)
QuietCoolTestCommandButton = quiet_cool_ns.class_("QuietCoolTestCommandButton", button.Button)
QuietCoolRemoteListenerSwitch = quiet_cool_ns.class_(
    "QuietCoolRemoteListenerSwitch", switch.Switch
)
QuietCoolTimerSelect = quiet_cool_ns.class_("QuietCoolTimerSelect", select.Select)
QuietCoolStatusPollingSwitch = quiet_cool_ns.class_(
    "QuietCoolStatusPollingSwitch", switch.Switch
)
QuietCoolPollingIntervalNumber = quiet_cool_ns.class_(
    "QuietCoolPollingIntervalNumber", number.Number
)
QuietCoolLearningStatus = quiet_cool_ns.class_(
    "QuietCoolLearningStatus", cg.Component, text_sensor.TextSensor
)
QuietCoolFrequencyNumber = quiet_cool_ns.class_("QuietCoolFrequencyNumber", number.Number)
CENTER_FREQUENCY_NUMBER_SCHEMA = number.number_schema(
    QuietCoolFrequencyNumber, unit_of_measurement="MHz", entity_category="config"
).extend({cv.Optional(CONF_NAME, default="QuietCool Center Frequency"): cv.string})
DEVIATION_NUMBER_SCHEMA = number.number_schema(
    QuietCoolFrequencyNumber, unit_of_measurement="kHz", entity_category="config"
).extend({cv.Optional(CONF_NAME, default="QuietCool Frequency Deviation"): cv.string})
POLLING_INTERVAL_SCHEMA = number.number_schema(
    QuietCoolPollingIntervalNumber,
    unit_of_measurement="s",
    entity_category="config",
).extend({cv.Optional(CONF_NAME, default="QuietCool Poll Interval"): cv.string})

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["button", "number", "select", "sensor", "switch", "text_sensor"]
FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "quiet_cool", require_mosi=True, require_miso=True
)


def validate_remote_id(value):
    value = cv.ensure_list(cv.hex_uint8_t)(value)
    if len(value) != 7:
        raise cv.Invalid("remote_id must contain exactly 7 bytes")
    return value


def validate_remote_ids(value):
    return [validate_remote_id(remote_id) for remote_id in cv.ensure_list(value)]


CONFIG_SCHEMA = fan.fan_schema(QuietCoolFan).extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(QuietCoolFan),
        cv.Required(CONF_GDO0_PIN): cv.uint8_t,
        cv.Required(CONF_GDO2_PIN): cv.uint8_t,
        cv.Optional(CONF_REMOTE_ID): validate_remote_id,
        cv.Optional(CONF_SPEED_COUNT, default=3): cv.one_of(2, 3, int=True),
        cv.Optional(CONF_FREQ_MHZ): cv.float_,
        cv.Optional(CONF_DEVIATION_KHZ): cv.float_,
        cv.Optional(CONF_LEARN_BUTTON, default={}): button.button_schema(
            QuietCoolLearnButton, entity_category="config"
        ).extend({cv.Optional(CONF_NAME, default="QuietCool Learn Remote"): cv.string}),
        cv.Optional(CONF_REMOTE_LISTENER): switch.switch_schema(
            QuietCoolRemoteListenerSwitch, entity_category="config"
        ).extend(
            {
                cv.Optional(CONF_NAME, default="Listen to Physical Remote"): cv.string,
                cv.Optional(CONF_ADDITIONAL_REMOTE_IDS, default=[]): validate_remote_ids,
            }
        ),
        cv.Optional(CONF_STATUS_POLLING, default={}): cv.Schema(
            {
                cv.Optional(CONF_POLL_ENABLED, default=True): cv.boolean,
                cv.Optional(CONF_POLL_INTERVAL_SECONDS, default=30): cv.int_range(
                    min=10, max=3600
                ),
                cv.Optional(CONF_POLL_SWITCH, default={}): switch.switch_schema(
                    QuietCoolStatusPollingSwitch, entity_category="config"
                ).extend(
                    {cv.Optional(CONF_NAME, default="Poll Fan Status"): cv.string}
                ),
                cv.Optional(CONF_POLL_INTERVAL, default={}): POLLING_INTERVAL_SCHEMA,
            }
        ),
        cv.Optional(CONF_TIMER): cv.Schema(
            {
                cv.Optional(CONF_TIMER_CONTROL, default={}): select.select_schema(
                    QuietCoolTimerSelect, entity_category="config"
                ).extend(
                    {cv.Optional(CONF_NAME, default="QuietCool Timer"): cv.string}
                ),
                cv.Optional(CONF_TIMER_REMAINING, default={}): sensor.sensor_schema(
                    unit_of_measurement="h",
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_DURATION,
                    state_class=STATE_CLASS_MEASUREMENT,
                ).extend(
                    {cv.Optional(CONF_NAME, default="QuietCool Time Remaining"): cv.string}
                ),
            }
        ),
        cv.Optional(CONF_TEST_BUTTONS): cv.ensure_list(
            button.button_schema(
                QuietCoolTestCommandButton, entity_category="config"
            ).extend(
                {cv.Required(CONF_COMMAND): cv.hex_uint8_t}
            )
        ),
        cv.Optional(CONF_FREQUENCY_CONTROLS): cv.Schema(
            {
                cv.Optional(CONF_CENTER_NUMBER): CENTER_FREQUENCY_NUMBER_SCHEMA,
                cv.Optional(CONF_DEVIATION_NUMBER): DEVIATION_NUMBER_SCHEMA,
            }
        ),
        cv.Optional(CONF_LEARNING_STATUS, default={}): text_sensor.text_sensor_schema(
            QuietCoolLearningStatus, entity_category="diagnostic"
        ).extend({cv.Optional(CONF_NAME, default="QuietCool Learning Status"): cv.string}),
        cv.Optional(CONF_RX_PACKET_SENSOR, default={}): text_sensor.text_sensor_schema(
            entity_category="diagnostic"
        ).extend({cv.Optional(CONF_NAME, default="QuietCool RF Activity"): cv.string}),
    }
).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema(cs_pin_required=True))


async def to_code(config):
    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("SPI", None)

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
    await spi.register_spi_device(var, config)

    cs_num = config[spi.CONF_CS_PIN]["number"]
    cg.add(var.set_pins(cs_num, config[CONF_GDO0_PIN], config[CONF_GDO2_PIN]))
    spi_config = next(
        bus for bus in CORE.config["spi"] if bus[CONF_ID] == config[spi.CONF_SPI_ID]
    )
    cg.add(
        var.set_spi_pins(
            spi_config[CONF_CLK_PIN]["number"],
            spi_config[CONF_MISO_PIN]["number"],
            spi_config[CONF_MOSI_PIN]["number"],
        )
    )
    if CONF_REMOTE_ID in config:
        cg.add(var.set_remote_id(config[CONF_REMOTE_ID]))
    cg.add(var.set_speed_count(config[CONF_SPEED_COUNT]))
    cg.add(
        var.set_frequencies(
            config.get(CONF_FREQ_MHZ, 433.897), config.get(CONF_DEVIATION_KHZ, 10.0)
        )
    )

    if CONF_FREQUENCY_CONTROLS in config:
        frequency_controls = config[CONF_FREQUENCY_CONTROLS]
    else:
        frequency_controls = {}
        fan_id = config[CONF_OUTPUT_ID].id
        frequency_controls[CONF_CENTER_NUMBER] = CENTER_FREQUENCY_NUMBER_SCHEMA(
            {
                CONF_ID: f"{fan_id}_center_frequency",
                CONF_NAME: "QuietCool Center Frequency",
            }
        )
        frequency_controls[CONF_DEVIATION_NUMBER] = DEVIATION_NUMBER_SCHEMA(
            {
                CONF_ID: f"{fan_id}_frequency_deviation",
                CONF_NAME: "QuietCool Frequency Deviation",
            }
        )
    center_frequency_is_fixed = (
        CONF_FREQ_MHZ in config and CONF_CENTER_NUMBER not in frequency_controls
    )
    deviation_is_fixed = (
        CONF_DEVIATION_KHZ in config and CONF_DEVIATION_NUMBER not in frequency_controls
    )
    cg.add(var.set_frequency_fixed(center_frequency_is_fixed, deviation_is_fixed))
    if CONF_CENTER_NUMBER in frequency_controls:
        number_config = frequency_controls[CONF_CENTER_NUMBER]
        frequency_number = await number.new_number(
            number_config, min_value=433.0, max_value=434.5, step=0.001
        )
        cg.add(frequency_number.set_parent(var, True))
        cg.add(var.set_center_freq_number(frequency_number))
    if CONF_DEVIATION_NUMBER in frequency_controls:
        number_config = frequency_controls[CONF_DEVIATION_NUMBER]
        frequency_number = await number.new_number(
            number_config, min_value=1.0, max_value=100.0, step=0.5
        )
        cg.add(frequency_number.set_parent(var, False))
        cg.add(var.set_deviation_number(frequency_number))

    learn_button = await button.new_button(config[CONF_LEARN_BUTTON])
    cg.add(learn_button.set_parent(var))

    if CONF_REMOTE_LISTENER in config:
        listener_config = config[CONF_REMOTE_LISTENER]
        additional_remote_ids = listener_config[CONF_ADDITIONAL_REMOTE_IDS]
        if additional_remote_ids:
            flattened_ids = [byte for remote_id in additional_remote_ids for byte in remote_id]
            cg.add(var.set_additional_remote_ids(flattened_ids))
        listener_switch = await switch.new_switch(listener_config)
        cg.add(listener_switch.set_parent(var))
        cg.add(var.set_remote_listener_switch(listener_switch))

    if CONF_STATUS_POLLING in config:
        polling_config = config[CONF_STATUS_POLLING]
        cg.add(
            var.set_status_polling_config(
                polling_config[CONF_POLL_ENABLED],
                polling_config[CONF_POLL_INTERVAL_SECONDS],
            )
        )
        polling_switch = await switch.new_switch(polling_config[CONF_POLL_SWITCH])
        cg.add(polling_switch.set_parent(var))
        cg.add(var.set_status_polling_switch(polling_switch))
        polling_interval = await number.new_number(
            polling_config[CONF_POLL_INTERVAL], min_value=10, max_value=3600, step=10
        )
        cg.add(polling_interval.set_parent(var))
        cg.add(var.set_polling_interval_number(polling_interval))

    if CONF_TIMER in config:
        timer_config = config[CONF_TIMER]
        timer_select = await select.new_select(
            timer_config[CONF_TIMER_CONTROL],
            options=["No Timer", "1 hour", "2 hours", "4 hours", "8 hours", "12 hours"],
        )
        cg.add(timer_select.set_parent(var))
        cg.add(var.set_timer_select(timer_select))
        remaining_sensor = await sensor.new_sensor(timer_config[CONF_TIMER_REMAINING])
        cg.add(var.set_timer_remaining_sensor(remaining_sensor))

    for test_button_config in config.get(CONF_TEST_BUTTONS, []):
        test_button = await button.new_button(test_button_config)
        cg.add(test_button.set_parent(var))
        cg.add(test_button.set_command(test_button_config[CONF_COMMAND]))

    status_sensor = await text_sensor.new_text_sensor(config[CONF_LEARNING_STATUS])
    await cg.register_component(status_sensor, config[CONF_LEARNING_STATUS])
    cg.add(var.set_learning_status_sensor(status_sensor))

    rx_packet_sensor = await text_sensor.new_text_sensor(config[CONF_RX_PACKET_SENSOR])
    cg.add(var.set_rx_activity_sensor(rx_packet_sensor))
