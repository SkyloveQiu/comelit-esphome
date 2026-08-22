import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_NAME
from .. import comelit_intercom_ns, ComelitIntercom, CONF_COMELIT_ID

ComelitIntercomTextSensor = comelit_intercom_ns.class_(
    "ComelitIntercomTextSensor", text_sensor.TextSensor
)

CONF_COMMANDS = "commands"
CONF_ADDRESSES = "addresses"
CONF_AUTO_CLEAR = "auto_clear"

DEPENDENCIES = ["comelit_intercom"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    ComelitIntercomTextSensor, icon="mdi:message-text"
).extend(
    {
        cv.GenerateID(CONF_COMELIT_ID): cv.use_id(ComelitIntercom),
        cv.Optional(CONF_COMMANDS): cv.ensure_list(cv.int_range(min=0, max=62)),
        cv.Optional(CONF_ADDRESSES): cv.ensure_list(cv.int_range(min=0, max=255)),
        cv.Optional(CONF_NAME, default="Last command"): cv.string,
        cv.Optional(CONF_AUTO_CLEAR, default="10s"): cv.positive_time_period_seconds,
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    # An unset filter stays an empty list, which accepts everything.
    if CONF_COMMANDS in config:
        cg.add(var.set_commands(config[CONF_COMMANDS]))
    if CONF_ADDRESSES in config:
        cg.add(var.set_addresses(config[CONF_ADDRESSES]))
    cg.add(var.set_auto_off(config[CONF_AUTO_CLEAR]))
    comelit_intercom = await cg.get_variable(config[CONF_COMELIT_ID])
    cg.add(comelit_intercom.register_listener(var))
