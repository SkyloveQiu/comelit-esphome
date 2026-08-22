import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import event
from esphome.const import CONF_NAME
from .. import comelit_intercom_ns, ComelitIntercom, CONF_COMELIT_ID

ComelitIntercomEvent = comelit_intercom_ns.class_("ComelitIntercomEvent", event.Event)

CONF_COMMANDS = "commands"
CONF_ADDRESS = "address"

DEPENDENCIES = ["comelit_intercom"]

# Command 63 is bus noise and is dropped before the listeners are notified, so it can
# never be reported and is left out of the declared event types.
ALL_COMMANDS = list(range(0, 63))

CONFIG_SCHEMA = event.event_schema(ComelitIntercomEvent).extend(
    {
        cv.GenerateID(CONF_COMELIT_ID): cv.use_id(ComelitIntercom),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.int_),
        cv.Optional(CONF_COMMANDS): cv.ensure_list(cv.int_range(min=0, max=62)),
        cv.Optional(CONF_NAME, default="Intercom event"): cv.string,
    }
)


async def to_code(config):
    # An event entity has to declare every type it can ever report at compile time.
    # Keep this format in sync with ComelitIntercomEvent::on_command(), which reports
    # the plain decimal command number.
    event_types = [str(command) for command in config.get(CONF_COMMANDS, ALL_COMMANDS)]
    var = await event.new_event(config, event_types=event_types)
    template_ = await cg.templatable(config[CONF_ADDRESS], [], cg.uint16)
    cg.add(var.set_address(template_))
    # An unset filter stays an empty list, which accepts every command.
    if CONF_COMMANDS in config:
        cg.add(var.set_commands(config[CONF_COMMANDS]))
    comelit_intercom = await cg.get_variable(config[CONF_COMELIT_ID])
    cg.add(comelit_intercom.register_listener(var))
