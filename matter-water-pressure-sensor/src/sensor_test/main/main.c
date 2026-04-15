#include <stdio.h>
#include <esp_log.h>
#include "../../matter_water_pressure_sensor/main/i2c_wp_sensor.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "trying to initialize sensor");

    if (!i2c_wp_sensor_init(20, 19)) 
    {
        ESP_LOGE(TAG, "failed to initialize sensor");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_system_abort("no sensor");
    }

    ESP_LOGI(TAG, "sensor iniialized");

    for (;;) 
    {
        float pressure_bar, temp_c;

        if (i2c_wp_sensor_read(&pressure_bar, &temp_c))
            ESP_LOGI(TAG, "measure: P=%f, T=%f", pressure_bar, temp_c);
        else
            ESP_LOGW(TAG, "measure: fail");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
