#include <application.h>

#define RELAY_PUB_NO_CHANGE_INTERVAL (5 * 60 * 1000)

#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (15 * 60 * 1000)
#define TEMPERATURE_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_INTERVAL (10 * 1000)

uint8_t relay_pin1 = BC_GPIO_P15;
uint8_t relay_pin2 = BC_GPIO_P14;
uint8_t relay_pin_state = BC_GPIO_P13;
uint8_t bell_pin = BC_GPIO_P12;
uint8_t button_pin = BC_GPIO_P0;
bc_scheduler_task_id_t switch_on_task;
bc_scheduler_task_id_t switch_off_task;
bc_switch_t relay_state;
bc_switch_t bell_state;

// Thermometer instance
bc_tmp112_t tmp112;
static event_param_t temperature_event_param = { .next_pub = 0 };

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);
static void switch_on_event_handler(void *param);
static void switch_off_event_handler(void *param);
void relay_send_state();
void relay_state_event_handler(bc_switch_t *self, bc_switch_event_t event, void *event_param);
void bell_state_event_handler(bc_switch_t *self, bc_switch_event_t event, void *event_param);
void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param);

void application_init(void)
{
    bc_radio_init(BC_RADIO_MODE_NODE_LISTENING);

    bc_gpio_init(relay_pin1);
    bc_gpio_set_mode(relay_pin1, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(relay_pin1, 0);
    bc_gpio_init(relay_pin2);
    bc_gpio_set_mode(relay_pin2, BC_GPIO_MODE_OUTPUT);
    bc_gpio_set_output(relay_pin2, 0);

    bc_switch_init(&relay_state, relay_pin_state, BC_SWITCH_TYPE_NO, BC_SWITCH_PULL_DOWN_DYNAMIC);
    bc_switch_set_event_handler(&relay_state, relay_state_event_handler, NULL);

    bc_switch_init(&bell_state, bell_pin, BC_SWITCH_TYPE_NO, BC_SWITCH_PULL_DOWN_DYNAMIC);
    bc_switch_set_event_handler(&bell_state, bell_state_event_handler, NULL);
    bc_switch_set_debounce_time(&bell_state, 1000);

    static bc_button_t button;
    bc_button_init(&button, button_pin, BC_GPIO_PULL_DOWN, 0);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    switch_on_task = bc_scheduler_register(switch_on_event_handler, NULL, BC_TICK_INFINITY);
    switch_off_task = bc_scheduler_register(switch_off_event_handler, NULL, BC_TICK_INFINITY);

    // Initialize thermometer sensor on cloony
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, &temperature_event_param);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_INTERVAL);

    bc_radio_pairing_request(FIRMWARE, VERSION);
}

void application_task()
{
    relay_send_state();

    bc_scheduler_plan_current_relative(RELAY_PUB_NO_CHANGE_INTERVAL);
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_scheduler_plan_now(switch_on_task);
    }
}

static void switch_on_event_handler(void *param)
{
    uint8_t relay_state = bc_gpio_get_input(relay_pin_state);

    if (relay_state == 1)
    {
        bc_gpio_set_output(relay_pin1, 0);
        bc_gpio_set_output(relay_pin2, 1);
    }
    else
    {
        bc_gpio_set_output(relay_pin1, 1);
        bc_gpio_set_output(relay_pin2, 0);
    }

    bc_scheduler_plan_relative(switch_off_task, 100);
}

static void switch_off_event_handler(void *param)
{
    bc_gpio_set_output(relay_pin1, 0);
    bc_gpio_set_output(relay_pin2, 0);
}

void relay_send_state()
{
    bool state = bc_switch_get_state(&relay_state);
    bc_radio_pub_state(BC_RADIO_PUB_STATE_POWER_MODULE_RELAY, &state);
}

void relay_state_event_handler(bc_switch_t *self, bc_switch_event_t event, void *event_param)
{
    relay_send_state();
}

void bc_radio_node_on_state_set(uint64_t *id, uint8_t state_id, bool *state)
{
    if (state_id == BC_RADIO_NODE_STATE_POWER_MODULE_RELAY)
    {
        bc_scheduler_plan_now(switch_on_task);
    }
}

void bc_radio_node_on_state_get(uint64_t *id, uint8_t state_id)
{
    if (state_id == BC_RADIO_NODE_STATE_POWER_MODULE_RELAY)
    {
        relay_send_state();
    }
}

void bell_state_event_handler(bc_switch_t *self, bc_switch_event_t event, void *event_param)
{
    static uint16_t event_count = 0;

    bc_radio_pub_push_button(&event_count);

    event_count++;
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        if (bc_tmp112_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_temperature(param->channel, &value);

                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}
