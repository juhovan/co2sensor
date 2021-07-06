# CO2Sensor

CO2 and temperature + humidity + pressure sensor for Home Assistant using an unknown Chinese CO2 sensor CO2-C8S and Bosch BME280 on a ESP8266 NodeMCU.
CO2 Sensor uses same protocol and pinout as [DS-CO2-20 sensor](https://www.icbanq.com/icdownload/data/ICBShop/Board/DS-CO2-20%20series%20data%20manual_English_V2.9.pdf), this program uses serial interface only.

## Setup:

- Clone/download this repository
- Copy `include/config.h.example` as `include/config.h`
- Set your configuration in `config.h`
- Build/flash like any other PlatformIO project
- The RX pin of sensor should be connected to GPIO15 (pin labeled D8) and TX of sensor to GPIO13 (pin labeled D7)

For Home Assistant you'll want something like this in your configuration.yaml:

```
sensor:
  - platform: mqtt
    name: "CO2Sensor"
    state_topic: "CO2Sensor/state"
    availability_topic: "CO2Sensor/availability"
    unit_of_measurement: "ppm"
    value_template: "{{ value_json.co2 }}"
```

## Over The Air update:

Documentation: https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html#web-browser

Basic steps:

- Use PlatformIO: Build
- Browse to http://IP_ADDRESS/update or http://hostname.local/update
- Select .pio/build/nodemcuv2/firmware.bin from work directory as Firmware and press Update Firmware
