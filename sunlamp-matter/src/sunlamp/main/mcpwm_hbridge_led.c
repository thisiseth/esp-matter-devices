#include "mcpwm_hbridge_led.h"
#include <esp_log.h>
#include <driver/mcpwm_prelude.h>
#include <math.h>

static const char *TAG = "mcpwm_hbridge";

static uint32_t max_cmpr_value;
static uint32_t min_cmpr_value;

static volatile uint32_t set_point_ww;
static volatile uint32_t set_point_wc;

static mcpwm_timer_handle_t timer;
static mcpwm_oper_handle_t operator;
static mcpwm_cmpr_handle_t comparator_ww;
static mcpwm_cmpr_handle_t comparator_wc;
static mcpwm_gen_handle_t generator_ww;
static mcpwm_gen_handle_t generator_wc;

static bool tez_handler(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *event_data, void *user_data);

bool mcpwm_hbridge_led_init(const mcpwm_hbridge_led_config_t *config)
{
    if (!config)
        return false;

    uint32_t period_ticks = (1 << config->pwm_depth_bits) * 2;

    if (period_ticks <= 4)
        return false;

    max_cmpr_value = (1 << config->pwm_depth_bits) - 1;

    if (config->minimum_pulse_ns <= 1 && config->pwm_frequency == 0)
        return false;

    uint32_t minimum_pulse_ns = config->minimum_pulse_ns;
    
    if (minimum_pulse_ns < 1)
        minimum_pulse_ns = 1;

    uint32_t resolution_hz = (1000*1000*1000) / minimum_pulse_ns;

    min_cmpr_value = 1;

    if (config->pwm_frequency > 0)
    {
        float period_ns = (1000*1000*1000) / config->pwm_frequency;
        float single_width_pulse = period_ns / period_ticks;
        float min_duty = minimum_pulse_ns / single_width_pulse;

        if (min_duty > 1.0f)
            min_cmpr_value = ceilf(min_duty);

        if (min_cmpr_value > max_cmpr_value)
            return false;

        resolution_hz = config->pwm_frequency * period_ticks;
    }

    const mcpwm_timer_config_t timer_config = {
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN,
        .period_ticks = period_ticks,
        .intr_priority = 3
    };

    if (mcpwm_new_timer(&timer_config, &timer) != ESP_OK)
        return false;

    ESP_LOGI(TAG, "resulting PWM freq is ~%i hz, min duty: %i, max duty: %i", 
        resolution_hz / period_ticks, min_cmpr_value, max_cmpr_value);

    const mcpwm_operator_config_t operator_config = {};

    if (mcpwm_new_operator(&operator_config, &operator) != ESP_OK ||
        mcpwm_operator_connect_timer(operator, timer) != ESP_OK)
        return false;

    const mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tep = true, //update on tez conflicts with 'PDM' for ww comparator (that triggers on UP) when duty is 0=>x=>0 - the 'x' pulse disappears
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

    if (min_cmpr_value > 1)
    {
        mcpwm_timer_event_callbacks_t timer_callbacks = {
            .on_empty = tez_handler
        };

        if (mcpwm_timer_register_event_callbacks(timer, &timer_callbacks, NULL))
            return false;
    }

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

    if (min_cmpr_value == 1) //set_point and pulse skipping stuff not used
    {
        mcpwm_comparator_set_compare_value(comparator_ww, ww_value);
        mcpwm_comparator_set_compare_value(comparator_wc, wc_value);
    }
    else
    {
        set_point_ww = ww_value;
        set_point_wc = wc_value;
    }

    ESP_LOGD(TAG, "cmp set to ww: %i, wc: %i", ww_value, wc_value);

    return true;
}

static inline __attribute__((always_inline)) void do_pdm(uint32_t set_point, int32_t *acc, mcpwm_cmpr_handle_t cmpr)
{
    uint32_t duty = 0;
    
    (*acc) -= (int32_t)set_point;

    if ((*acc) <= 0)
    {
        duty = min_cmpr_value;
        (*acc) += (int32_t)duty;
    }

    mcpwm_comparator_set_compare_value(cmpr, duty);
}

#define IS_PDM(set_point) ((set_point) < min_cmpr_value && (set_point) != 0)

static bool IRAM_ATTR tez_handler(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *event_data, void *user_data)
{
    static int32_t pdm_acc_ww, pdm_acc_wc;
    static uint32_t prev_set_point_ww, prev_set_point_wc;

    uint32_t ww_value = set_point_ww, wc_value = set_point_wc;

    if (IS_PDM(ww_value) != IS_PDM(prev_set_point_ww))
        pdm_acc_ww = 0;

    if (IS_PDM(wc_value) != IS_PDM(prev_set_point_wc))
        pdm_acc_wc = 0;    
        
    prev_set_point_ww = ww_value;
    prev_set_point_wc = wc_value;

    if (IS_PDM(ww_value))
        do_pdm(ww_value, &pdm_acc_ww, comparator_ww);
    else
        mcpwm_comparator_set_compare_value(comparator_ww, ww_value);

    if (IS_PDM(wc_value))
        do_pdm(wc_value, &pdm_acc_wc, comparator_wc);
    else
        mcpwm_comparator_set_compare_value(comparator_wc, wc_value);

    return false;
}