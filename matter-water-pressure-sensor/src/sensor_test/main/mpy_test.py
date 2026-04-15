from machine import Pin, SoftI2C
from time import sleep_us


LED_BUILTIN = 15
LED_WS2812 = 8

SENSOR_SDA = 20
SENSOR_SCL = 19

SENSOR_I2C_ADDRESS = 0x78


led_builtin = Pin(LED_BUILTIN, mode=Pin.OUT, value=False)

i2c = SoftI2C(scl=SENSOR_SCL, sda=SENSOR_SDA, freq=100_000)


def measure():
    i2c.writeto(SENSOR_I2C_ADDRESS, bytes([0xAC]))
    
    while True:
        sleep_us(100)
        status = i2c.readfrom(SENSOR_I2C_ADDRESS, 1)[0]
        
        if (status & 0x60) == 0:
            break
        
    data = i2c.readfrom(SENSOR_I2C_ADDRESS, 6)[-5:]
    
    print(data)
    
    pressure_adc = ((data[0] << 16) + (data[1] << 8) + data[2]) / 16777216
    temp_adc = ((data[3] << 8) + data[4]) / 65536
    
    print(f'{pressure_adc}, {temp_adc}')
    
    #0.62332152 ~ 20c
    #0.73680116 ~ 0c
    
    temp = (125 - (-40))*(1-temp_adc) + (-40)
    pressure = pressure_adc * 10
    
    print(f'{pressure}, {temp}')
    
measure()
