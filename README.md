# esp-matter-devices

A collection of Matter devices i've built using [esp-matter](https://github.com/espressif/esp-matter/) SDK for **esp32** family SoCs.

Important: these are quick cheap and dirty 'working enough' implementations built on top of SDK samples, so consider them more like a source of useful insights, than ready to use polished products.

### Contents:

### [lunalamp-matter](/lunalamp-matter/)

Extended Color Light device for RGBWW strips, port of my HomeKit [lunalamp-esp32](https://github.com/thisiseth/lunalamp-esp32/). 
Transforms Matter XY/CT colours into RGBWW and uses LEDC PWM with external MOSFETs to drive a 12v led strip.

### [matter-water-pressure-sensor](/matter-water-pressure-sensor/)

todo

### doorbell-chime

todo

## How to
### Prerequisites

If using Windows, the most troublesome part is setting up the **esp-matter** SDK itself -- since **esp-matter** is basically an abstraction over the [CHIP](https://github.com/project-chip/connectedhomeip/) SDK, 
so native Windows build is not supported.

WSL2 is fine, but you have to store **esp-matter** inside the native linux fs -- Windows to linux fs passthrough won't work.


