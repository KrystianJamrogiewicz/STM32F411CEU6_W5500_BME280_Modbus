# STM32 Modbus TCP Weather Station

This project is a weather station based on the STM32F411CEU6 (Blackpill) microcontroller. The device reads environmental data (temperature, humidity, pressure) from a BME280 sensor and makes it available on the local network as a Modbus TCP server via the W5500 hardware Ethernet module.

---

## Tech Stack

**Hardware & Environment:**
* **MCU:** STM32F411CEU6 (BlackPill)
* **Environment:** STM32CubeIDE (C)

**Modules & Devices:**
* **Ethernet:** W5500 module (SPI communication)
* **Sensor:** BME280 (I2C communication)