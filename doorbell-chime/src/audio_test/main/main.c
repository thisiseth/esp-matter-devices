#include <stdio.h>
#include <esp_log.h>
#include "../../doorbell_chime/main/audio.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "trying to initialize audio");

    //if (!audio_init(AUDIO_BACKEND_PCM_TO_PDM, 25)) 
    if (!audio_init(AUDIO_BACKEND_DAC, 26)) 
    {
        ESP_LOGE(TAG, "failed to initialize audio");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_system_abort("no audio");
    }

    ESP_LOGI(TAG, "audio iniialized");

    for (;;) 
    {
        audio_play();

        vTaskDelay(pdMS_TO_TICKS(22000));
    }
}
