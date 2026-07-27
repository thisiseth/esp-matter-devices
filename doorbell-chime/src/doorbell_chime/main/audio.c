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

#define AUDIO_WAV_SAMPLE_RATE 44100

extern const uint8_t audio_wav_start[] asm("_binary_audio_wav_start");
extern const uint8_t audio_wav_end[] asm("_binary_audio_wav_end");

static const int16_t *audio_wav_samples;
static uint32_t audio_wav_samples_len;

static int audio_backend = -1;

static TaskHandle_t audio_task;

#ifdef CONFIG_SOC_DAC_SUPPORTED
static dac_continuous_handle_t dac_handle;
#endif

#ifdef CONFIG_SOC_I2S_SUPPORTS_PCM2PDM
static i2s_chan_handle_t i2s_handle;
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
        header.sample_rate != AUDIO_WAV_SAMPLE_RATE ||
        header.bits_per_sample != 16 ||
        header.block_align != 2 || //channels*bitdepth/8
        header.byte_rate != (AUDIO_WAV_SAMPLE_RATE * 2)) 
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

    ESP_LOGI(TAG, "wav file size: %dk, length: %.2f s", (audio_wav_samples_len)/1024, (2*audio_wav_samples_len)/(float)header.byte_rate);

    return true;
}

static void audio_func(void *data)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        int current_sample_pos = 0;

#ifdef CONFIG_SOC_I2S_SUPPORTS_PCM2PDM
        if (audio_backend == AUDIO_BACKEND_PCM_TO_PDM)
            i2s_channel_enable(i2s_handle);
#endif

        while (current_sample_pos < audio_wav_samples_len)
        {
            if (ulTaskNotifyTake(pdTRUE, 0))
                current_sample_pos = 0; //another play queued - restart

            int samples_available = audio_wav_samples_len - current_sample_pos;

            if (samples_available > 1024)
                samples_available = 1024;

            switch (audio_backend)
            {
#ifdef CONFIG_SOC_DAC_SUPPORTED
                case AUDIO_BACKEND_DAC:
                {
                    uint8_t dac_samples[1024];

                    for (int i = 0; i < samples_available; ++i) 
                        dac_samples[i] = (audio_wav_samples[current_sample_pos + i] + 32768) >> 8;
                    
                    dac_continuous_write(dac_handle, dac_samples, samples_available, NULL, -1);
                    break;
                }
#endif
#ifdef CONFIG_SOC_I2S_SUPPORTS_PCM2PDM
                case AUDIO_BACKEND_PCM_TO_PDM:
                {
                    i2s_channel_write(i2s_handle, audio_wav_samples + current_sample_pos, samples_available * 2, NULL, portMAX_DELAY);
                    break;
                }
#endif
                default:
                    esp_system_abort("not implemented audio backend @ audio_func");
            }

            current_sample_pos += samples_available;
        }

#ifdef CONFIG_SOC_I2S_SUPPORTS_PCM2PDM
        if (audio_backend == AUDIO_BACKEND_PCM_TO_PDM)
        {
            //wait until buffered audio is played
            vTaskDelay(100 / portTICK_PERIOD_MS);
            i2s_channel_disable(i2s_handle);
        }
#endif

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

                .freq_hz = AUDIO_WAV_SAMPLE_RATE,
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
            i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
            chan_cfg.dma_desc_num = 4;
            chan_cfg.dma_frame_num = 1024;
            chan_cfg.auto_clear = true;

            if (i2s_new_channel(&chan_cfg, &i2s_handle, NULL) != ESP_OK) 
            {
                ESP_LOGE(TAG, "i2s_new_channel failed");
                return false;
            }

            const i2s_pdm_tx_config_t pdm_cfg = 
            {
                .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(AUDIO_WAV_SAMPLE_RATE), //tried using 'dac' preset but it makes audio slower for 44.1k
                .slot_cfg = I2S_PDM_TX_SLOT_PCM_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),

                .gpio_cfg = 
                {
                    .clk = -1,
                    .dout = output_pin
                },
            };

            if (i2s_channel_init_pdm_tx_mode(i2s_handle, &pdm_cfg) != ESP_OK) 
            {
                ESP_LOGE(TAG, "PDM TX init failed");
                return false;
            }

            gpio_set_drive_capability(output_pin, GPIO_DRIVE_CAP_3);

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