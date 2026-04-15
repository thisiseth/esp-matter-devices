/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <esp_matter.h>
#include <app_priv.h>
#include <common_macros.h>

#include <device.h>
#include <led_driver.h>
#include <button_gpio.h>

#include <freertos/task.h>

#include "i2c_wp_sensor.h"
#include "onboard_led_helper.h"

using namespace chip::app::Clusters;
using namespace esp_matter;

#define SENSOR_I2C_SDA 20
#define SENSOR_I2C_SCL 19

static const char *TAG = "app_driver";

static led_driver_handle_t onboard_led;
static uint16_t pressure_ep, temp_ep;

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
}

bool update_sensor_values(void)
{
    float pressure_bar, temp_c;

    esp_err_t err = ESP_OK;

    if (!i2c_wp_sensor_read(&pressure_bar, &temp_c))
    {
        onboard_led_red(onboard_led);
        pressure_bar = I2C_WP_SENSOR_PRESSURE_BAR_MIN;
        temp_c = I2C_WP_SENSOR_TEMP_C_MIN;
        err = ESP_FAIL;
    }
    else if (pressure_bar < 2.0f)
        onboard_led_yellow(onboard_led);
    else
        onboard_led_green(onboard_led);

    esp_matter_attr_val_t val = esp_matter_nullable_int16(SENSOR_PRESSURE_TO_MATTER(pressure_bar));
    err |= attribute::update(pressure_ep, PressureMeasurement::Id, PressureMeasurement::Attributes::MeasuredValue::Id, &val);

    val = esp_matter_nullable_int16(SENSOR_TEMP_TO_MATTER(temp_c));
    err |= attribute::update(temp_ep, TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &val);

    return err == ESP_OK;
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    return ESP_OK;
}

esp_err_t app_driver_wp_sensor_init(uint16_t endpoint_pressure, uint16_t endpoint_temp)
{
    if (!i2c_wp_sensor_init((gpio_num_t)SENSOR_I2C_SDA, (gpio_num_t)SENSOR_I2C_SCL))
        return ESP_FAIL;

    pressure_ep = endpoint_pressure;
    temp_ep = endpoint_temp;

    if (!update_sensor_values())
        return ESP_FAIL;

    return ESP_OK;
}

app_driver_handle_t app_driver_light_init()
{
    /* Initialize led */
    led_driver_config_t config = led_driver_get_config();
    onboard_led = led_driver_init(&config);
    return (app_driver_handle_t)onboard_led;
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = button_driver_get_config();

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }

    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, app_driver_button_toggle_cb, NULL);
    return (app_driver_handle_t)handle;
}
