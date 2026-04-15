#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <driver/gpio.h>

#define I2C_WP_SENSOR_PRESSURE_BAR_MIN 0.0f
#define I2C_WP_SENSOR_PRESSURE_BAR_MAX 10.0f
#define I2C_WP_SENSOR_TEMP_C_MIN -40.0f
#define I2C_WP_SENSOR_TEMP_C_MAX 125.0f

bool i2c_wp_sensor_init(gpio_num_t sda, gpio_num_t scl);
bool i2c_wp_sensor_read(float *pressure_bar, float *temp_c);

#ifdef __cplusplus
}
#endif