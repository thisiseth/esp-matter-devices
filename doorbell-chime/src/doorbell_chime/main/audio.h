#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <sdkconfig.h>
#include <driver/gpio.h>

typedef enum {
    AUDIO_BACKEND_NONE = 0, //dummy
    AUDIO_BACKEND_DAC = 1, //esp32, esp32-s2
    AUDIO_BACKEND_PCM_TO_PDM = 2 //everywhere except esp32-s2
} audio_backend_t;

bool audio_init(audio_backend_t backend, gpio_num_t output_pin);
void audio_play(void);

#ifdef __cplusplus
}
#endif