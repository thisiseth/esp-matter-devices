#include "i2c_wp_sensor.h"
#include <esp_log.h>
#include <esp_err.h>
#include <driver/i2c_master.h>
#include "esp_rom_sys.h"

static const char *TAG = "i2c_wp_sensor";

#define I2C_ADDRESS 0x78
#define I2C_FREQ 100000
#define I2C_TIMEOUT_MS 100

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t i2c_dev;

static bool i2c_wp_sensor_read_status(bool full, uint8_t data[6])
{
    esp_err_t err = i2c_master_receive(i2c_dev, data, full ? 6 : 1, I2C_TIMEOUT_MS);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "sensor_read_status: failed to receive sensor status, full=%d: %s", full, esp_err_to_name(err));
        return false;
    }

    return true;
}

static bool i2c_wp_sensor_start_measurement()
{
    uint8_t data[6];
    
    for (;;)
    {
        if (!i2c_wp_sensor_read_status(false, data))
        {
            ESP_LOGW(TAG, "start_measurement: failed to read status");
            return false;
        }

        if (!(data[0] & 0x60))
            break;

        esp_rom_delay_us(100);
    }

    uint8_t buf[] = { 0xAC };

    esp_err_t err = i2c_master_transmit(i2c_dev, buf, 1, I2C_TIMEOUT_MS);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "start_measurement: failed to transmit start command: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

bool i2c_wp_sensor_init(gpio_num_t sda, gpio_num_t scl)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDRESS,
        .scl_speed_hz = I2C_FREQ
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev));

    uint8_t data[6];

    if (!i2c_wp_sensor_read_status(false, data))
    {
        ESP_LOGE(TAG, "sensor_init: failed to read status");
        return false;
    }

    ESP_LOGI(TAG, "sensor_init: reported status byte: 0x%x", data[0]);

    return true;
}

bool i2c_wp_sensor_read(float *pressure_bar, float *temp_c)
{
    if (!i2c_wp_sensor_start_measurement()) 
    {
        ESP_LOGW(TAG, "sensor_read: failed to start measurement");
        return false;
    }

    uint8_t data[6];
    
    for (;;)
    {
        esp_rom_delay_us(200);

        if (!i2c_wp_sensor_read_status(true, data)) 
        {
            ESP_LOGW(TAG, "sensor_read: failed to read status");
            return false;
        }

        if (!(data[0] & 0x60))
            break;
    }

    uint32_t pressure_raw = (data[1] << 16) + (data[2] << 8) + data[3];
    uint32_t temp_raw = (data[4] << 8) + data[5];

    *pressure_bar = (pressure_raw / (float)(0x1000000))*(I2C_WP_SENSOR_PRESSURE_BAR_MAX-I2C_WP_SENSOR_PRESSURE_BAR_MIN) + I2C_WP_SENSOR_PRESSURE_BAR_MIN;
    *temp_c = (1.0f - (temp_raw / (float)(0x10000)))*(I2C_WP_SENSOR_TEMP_C_MAX-I2C_WP_SENSOR_TEMP_C_MIN) + I2C_WP_SENSOR_TEMP_C_MIN;

    return true;
}