#include "audio.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef CONFIG_SOC_DAC_SUPPORTED
#include "soc/dac_channel.h"
#include <driver/dac_continuous.h>
#endif

#ifdef CONFIG_SOC_I2S_SUPPORTS_PCM2PDM
#include <driver/i2s_pdm.h>
#endif

typedef struct __attribute__((packed)) {
    char riff_id[4];
    uint32_t riff_size;
    char wave_id[4];
    char fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_id[4];
    uint32_t data_size;
} wav_header_t;

_Static_assert(sizeof(wav_header_t) == 44, "Unexpected WAV header size");

const char *TAG = "audio";

extern const uint8_t audio_wav_start[] asm("_binary_audio_wav_start");
extern const uint8_t audio_wav_end[] asm("_binary_audio_wav_end");

static const int16_t *audio_wav_samples;
static uint32_t audio_wav_samples_len;

static int audio_backend = -1;

static TaskHandle_t audio_task;

#ifdef CONFIG_SOC_DAC_SUPPORTED
static dac_continuous_handle_t dac_handle;
#endif

static bool parse_wav_header(void)
{
    int32_t audio_wav_len = audio_wav_end - audio_wav_start;

    if (audio_wav_len < sizeof(wav_header_t)) 
    {
        ESP_LOGE(TAG, "wav length %d less than the header size", audio_wav_len);
        return false;
    }

    wav_header_t header;
    memcpy(&header, audio_wav_start, sizeof(header));

    if (memcmp(header.riff_id, "RIFF", 4) != 0 ||
        memcmp(header.wave_id, "WAVE", 4) != 0 ||
        memcmp(header.fmt_id,  "fmt ", 4) != 0 ||
        memcmp(header.data_id, "data", 4) != 0) 
    {
        ESP_LOGE(TAG, "wav header string constants incorrect");
        return false;
    }

    if (header.fmt_size != 16 ||
        header.audio_format != 1 || //pcm
        header.channels != 1 ||
        header.sample_rate != 44100 ||
        header.bits_per_sample != 16 ||
        header.block_align != 2 || //channels*bitdepth/8
        header.byte_rate != 88200) 
    {
        ESP_LOGE(TAG, "wav format is not supported: should be simple mono 16-bit 44.1k PCM");
        return false;
    }

    if (header.data_size > audio_wav_len - sizeof(wav_header_t) ||
        (header.data_size % sizeof(int16_t)) != 0) 
    {
        ESP_LOGE(TAG, "wav file size doesnt match header value");
        return false;
    }

    audio_wav_samples = (const int16_t*)(audio_wav_start + sizeof(wav_header_t));
    audio_wav_samples_len = header.data_size / 2;

    ESP_LOGI(TAG, "wav file size: %dk, length: %.2f s", audio_wav_samples_len/1024, audio_wav_samples_len/(float)header.byte_rate);

    return true;
}

static void audio_func(void *data)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (audio_backend == AUDIO_BACKEND_NONE)
            continue;

        int current_sample_pos = 0;

        while (current_sample_pos < audio_wav_samples_len)
        {
            if (ulTaskNotifyTake(pdTRUE, 0))
                current_sample_pos = 0; //another play queued - restart

            switch (audio_backend)
            {
#ifdef CONFIG_SOC_DAC_SUPPORTED
                case AUDIO_BACKEND_DAC:
                {
                    uint8_t dac_samples[1024];

                    int samples_available = audio_wav_samples_len - current_sample_pos;

                    if (samples_available > 1024)
                        samples_available = 1024;

                    for (int i = 0; i < samples_available; ++i) 
                        dac_samples[i] = (audio_wav_samples[current_sample_pos + i] + 32768) >> 8;
                    
                    dac_continuous_write(dac_handle, dac_samples, samples_available, NULL, -1);
                    current_sample_pos += samples_available;
                    break;
                }
#endif
#ifdef CONFIG_SOC_I2S_SUPPORTS_PCM2PDM
                case AUDIO_BACKEND_PCM_TO_PDM:
                {
                    break;
                }
#endif
                default:
                    esp_system_abort("not implemented audio backend @ audio_func");
            }
        }

        ESP_LOGI(TAG, "audio finished playing");
    }
}

bool audio_init(audio_backend_t backend, gpio_num_t output_pin)
{
    if (audio_backend >= 0)
    {
        ESP_LOGE(TAG, "audio already initialized");
        return false;
    }

    if (!parse_wav_header())
        return false;

    switch (backend)
    {
        case AUDIO_BACKEND_NONE:
        {
            audio_backend = backend;
            return true;
        }
#ifdef CONFIG_SOC_DAC_SUPPORTED
        case AUDIO_BACKEND_DAC:
        {
            dac_channel_mask_t channels;

            switch (output_pin)
            {
#ifdef DAC_CHAN0_GPIO_NUM
                case DAC_CHAN0_GPIO_NUM:
                {
                    channels = DAC_CHANNEL_MASK_CH0;
                    break;
                }
#endif
#ifdef DAC_CHAN1_GPIO_NUM
                case DAC_CHAN1_GPIO_NUM:
                {
                    channels = DAC_CHANNEL_MASK_CH1;
                    break;
                }
#endif
                default:
                {
                    ESP_LOGE(TAG, "cant determine DAC channel for output_pin %d", output_pin);
                    return false;
                }
            }

            dac_continuous_config_t cfg = {
                .chan_mask = channels,

                .desc_num = 4,
                .buf_size = 1024,

                .freq_hz = 44100,
                .offset = 0,

                .clk_src = DAC_DIGI_CLK_SRC_APLL,
                .chan_mode = DAC_CHANNEL_MODE_SIMUL,
            };

            if (dac_continuous_new_channels(&cfg, &dac_handle) != ESP_OK || 
                dac_continuous_enable(dac_handle) != ESP_OK)
            {
                ESP_LOGE(TAG, "failed to init DAC");
                return false;
            }

            if (xTaskCreate(audio_func, "audio", 8192, NULL, 5, &audio_task) != pdPASS)
            {
                ESP_LOGE(TAG, "failed to create audio task");
                return false;
            }

            audio_backend = backend;
            return true;
        }
#endif
#ifdef CONFIG_SOC_I2S_SUPPORTS_PCM2PDM
        case AUDIO_BACKEND_PCM_TO_PDM:
        {
            if (xTaskCreate(audio_func, "audio", 8192, NULL, 5, &audio_task) != pdPASS)
            {
                ESP_LOGE(TAG, "failed to create audio task");
                return false;
            }
            
            audio_backend = backend;
            return true;
        }
#endif
        default:
        {
            ESP_LOGE(TAG, "unsupported audio backend: %d", backend);
            return false;
        }
    }
}

void audio_play(void)
{
    if (audio_backend < 0)
    {
        ESP_LOGE(TAG, "audio play called when not initialized");
        return;
    }

    ESP_LOGI(TAG, "audio play call");

    if (audio_task)
        xTaskNotifyGive(audio_task);
}