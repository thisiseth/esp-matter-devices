#include "mcpwm_hbridge_led.h"
#include <esp_log.h>
#include <driver/mcpwm_prelude.h>

static const char *TAG = "mcpwm_hbridge";

static int max_cmpr_value;

static mcpwm_timer_handle_t timer;
static mcpwm_oper_handle_t operator;
static mcpwm_cmpr_handle_t comparator_ww;
static mcpwm_cmpr_handle_t comparator_wc;
static mcpwm_gen_handle_t generator_ww;
static mcpwm_gen_handle_t generator_wc;

bool mcpwm_hbridge_led_init(const mcpwm_hbridge_led_config_t *config)
{
    if (!config)
        return false;

    int32_t period = (1 << config->pwm_depth_bits)*2;

    if (period <= 0)
        return false;

    max_cmpr_value = (1 << config->pwm_depth_bits) - 1;

    int32_t resolution_hz = (1000*1000*1000)/config->minimum_pulse_ns;

    if (resolution_hz <= 0)
        return false;

    const mcpwm_timer_config_t timer_config = {
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN,
        .period_ticks = period,
    };

    if (mcpwm_new_timer(&timer_config, &timer) != ESP_OK)
        return false;

    ESP_LOGI(TAG, "resulting PWM freq is ~%i hz", resolution_hz / period);

    const mcpwm_operator_config_t operator_config = {};

    if (mcpwm_new_operator(&operator_config, &operator) != ESP_OK ||
        mcpwm_operator_connect_timer(operator, timer) != ESP_OK)
        return false;

    const mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };

    if (mcpwm_new_comparator(operator, &comparator_config, &comparator_ww) != ESP_OK ||
        mcpwm_new_comparator(operator, &comparator_config, &comparator_wc) != ESP_OK)
        return false;

    if (mcpwm_comparator_set_compare_value(comparator_ww, 0) != ESP_OK || 
        mcpwm_comparator_set_compare_value(comparator_wc, 0) != ESP_OK)
        return false;

    const mcpwm_generator_config_t ww_gen_config = {
        .gen_gpio_num = config->pin_ww_plus
    };

    const mcpwm_generator_config_t wc_gen_config = {
        .gen_gpio_num = config->pin_wc_plus
    };

    if (mcpwm_new_generator(operator, &ww_gen_config, &generator_ww) != ESP_OK ||
        mcpwm_new_generator(operator, &wc_gen_config, &generator_wc) != ESP_OK)
        return false;
   
    //at TEZ WW->1, WC->0 => output state 10, ww is lit
    //at UP cmp_ww WC->1 => output state 11 (active low), idle
    //at DOWN cmp_wc WW->0 => output state 01, wc is lit

    if (mcpwm_generator_set_action_on_timer_event(generator_ww, 
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)) != ESP_OK ||
        mcpwm_generator_set_action_on_timer_event(generator_wc, 
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW)) != ESP_OK ||
        mcpwm_generator_set_action_on_compare_event(generator_wc,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_ww, MCPWM_GEN_ACTION_HIGH)) != ESP_OK ||
        mcpwm_generator_set_action_on_compare_event(generator_ww,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, comparator_wc, MCPWM_GEN_ACTION_LOW)) != ESP_OK)
        return false;

    if (mcpwm_timer_enable(timer) != ESP_OK || 
        mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP) != ESP_OK)
        return false;

    return true;
}

bool mcpwm_hbridge_led_set(float ww, float wc)
{
    if (ww < 0.0f || ww > 1.0f || wc < 0.0f || wc > 1.0f)
        return false;

    uint32_t ww_value = (uint32_t)(max_cmpr_value * ww), wc_value = (uint32_t)(max_cmpr_value * wc);

    mcpwm_comparator_set_compare_value(comparator_ww, ww_value);
    mcpwm_comparator_set_compare_value(comparator_wc, wc_value);

    ESP_LOGD(TAG, "cmp set to ww: %i, wc: %i", ww_value, wc_value);

    return true;
}
