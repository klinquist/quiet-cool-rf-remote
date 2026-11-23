import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, fan, number, spi, text_sensor
from esphome.const import CONF_CLK_PIN, CONF_ID, CONF_MISO_PIN, CONF_MOSI_PIN, CONF_NAME, CONF_OUTPUT_ID, CONF_SPEED_COUNT
from esphome.core import CORE
from .. import quiet_cool_ns

CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_REMOTE_ID = "remote_id"
CONF_FREQ_MHZ = "center_freq_mhz"
CONF_DEVIATION_KHZ = "deviation_khz"
CONF_LEARN_BUTTON = "learn_button"
CONF_LEARNING_STATUS = "learning_status"
CONF_TEST_BUTTONS = "test_buttons"
CONF_COMMAND = "command"
CONF_FREQUENCY_CONTROLS = "frequency_controls"
CONF_CENTER_NUMBER = "center_freq_mhz"
CONF_DEVIATION_NUMBER = "deviation_khz"

QuietCoolFan = quiet_cool_ns.class_("QuietCoolFan", cg.Component, fan.Fan, spi.SPIDevice)
QuietCoolLearnButton = quiet_cool_ns.class_("QuietCoolLearnButton", button.Button)
QuietCoolTestCommandButton = quiet_cool_ns.class_("QuietCoolTestCommandButton", button.Button)
QuietCoolLearningStatus = quiet_cool_ns.class_(
    "QuietCoolLearningStatus", cg.Component, text_sensor.TextSensor
)
QuietCoolFrequencyNumber = quiet_cool_ns.class_("QuietCoolFrequencyNumber", number.Number)
CENTER_FREQUENCY_NUMBER_SCHEMA = number.number_schema(
    QuietCoolFrequencyNumber, unit_of_measurement="MHz"
).extend({cv.Optional(CONF_NAME, default="QuietCool Center Frequency"): cv.string})
DEVIATION_NUMBER_SCHEMA = number.number_schema(
    QuietCoolFrequencyNumber, unit_of_measurement="kHz"
).extend({cv.Optional(CONF_NAME, default="QuietCool Frequency Deviation"): cv.string})

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["button", "number", "text_sensor"]
FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "quiet_cool", require_mosi=True, require_miso=True
)


def validate_remote_id(value):
    value = cv.ensure_list(cv.hex_uint8_t)(value)
    if len(value) != 7:
        raise cv.Invalid("remote_id must contain exactly 7 bytes")
    return value


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
            QuietCoolLearnButton
        ).extend({cv.Optional(CONF_NAME, default="QuietCool Learn Remote"): cv.string}),
        cv.Optional(CONF_TEST_BUTTONS): cv.ensure_list(
            button.button_schema(QuietCoolTestCommandButton).extend(
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
            QuietCoolLearningStatus
        ).extend({cv.Optional(CONF_NAME, default="QuietCool Learning Status"): cv.string}),
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
    center_frequency_is_fixed = CONF_FREQ_MHZ in config
    deviation_is_fixed = CONF_DEVIATION_KHZ in config
    cg.add(var.set_frequency_fixed(center_frequency_is_fixed, deviation_is_fixed))
    cg.add(
        var.set_frequencies(
            config.get(CONF_FREQ_MHZ, 433.897), config.get(CONF_DEVIATION_KHZ, 10.0)
        )
    )

    if CONF_FREQUENCY_CONTROLS in config:
        frequency_controls = config[CONF_FREQUENCY_CONTROLS]
    else:
        frequency_controls = {}
        if not center_frequency_is_fixed:
            frequency_controls[CONF_CENTER_NUMBER] = CENTER_FREQUENCY_NUMBER_SCHEMA({})
        if not deviation_is_fixed:
            frequency_controls[CONF_DEVIATION_NUMBER] = DEVIATION_NUMBER_SCHEMA({})
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

    for test_button_config in config.get(CONF_TEST_BUTTONS, []):
        test_button = await button.new_button(test_button_config)
        cg.add(test_button.set_parent(var))
        cg.add(test_button.set_command(test_button_config[CONF_COMMAND]))

    status_sensor = await text_sensor.new_text_sensor(config[CONF_LEARNING_STATUS])
    await cg.register_component(status_sensor, config[CONF_LEARNING_STATUS])
    cg.add(var.set_learning_status_sensor(status_sensor))
