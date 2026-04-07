# STM32 Modbus TCP Weather Station

This project is a weather station based on the STM32F411CEU6 (Blackpill) microcontroller. The device reads environmental data (temperature, humidity, pressure) from a BME280 sensor via I2C and makes it available on a local network as a **Modbus TCP server** using the W5500 Ethernet module via SPI.

---

## Tech Stack

**Hardware & Environment:**
* **MCU:** STM32F411CEU6 (BlackPill)
* **Firmware Development:** STM32CubeIDE (Bare-metal C)
* **Software Development:** Python 3.x (PyCharm IDE)

**Modules & Protocols:**
* **Ethernet:** W5500 module (**SPI** burst communication)
* **Sensor:** BME280 (**I2C** communication)
* **Diagnostics:** USB-TTL Converter (**UART** communication)
* **Network Protocol:** **Modbus TCP/IP**

---

## System Architecture & Finite State Machine (FSM)

The core logic of the application runs on a non-blocking Finite State Machine (FSM) utilizing `HAL_GetTick()` for timing. This ensures the microcontroller never freezes while waiting for network packets or sensor data.

### FSM Diagram
![State Machine Logic](docs/app_loop_fsm.drawio.svg)

**FSM States:**
1. **STATE_INIT:** Initializes peripherals, I2C, SPI, and Network settings.
2. **STATE_IDLE:** The main waiting room. Evaluates conditions (Timers and Interrupt flags) to transition to work states.
3. **STATE_NETWORK_PROCESS:** Parses incoming Modbus TCP frames and sends responses.
4. **STATE_READ_SENSOR:** Fetches data from the BME280 sensor and updates the Modbus Holding Registers.
5. **STATE_SENSOR_ERROR:** Diagnostics state triggered if the I2C sensor fails (toggles LED, outputs UART warnings).
6. **STATE_NETWORK_ERROR:** Critical failure state (e.g., W5500 IP Conflict). Halts normal operation and requires a reset.

---

## Hardware Interrupts (W5500 EXTI)

To save MCU CPU cycles, the network module does not use constant polling. Instead, the W5500 INT pin is connected to an STM32 EXTI (External Interrupt) pin. 
When a packet arrives, the W5500 pulls the INT pin low. The STM32 triggers the `HAL_GPIO_EXTI_Callback`, which calls `W5500_Process_Interrupt()`. This function smartly filters the interrupts, ignoring trivial events and only waking up the FSM if valid Modbus data is ready in the RAM (`Sn_IR_RECV`).

---

## Error Handling & Fault Tolerance

* **Hardware level:** Physical cable disconnection is detected by reading the W5500 PHY register.
* **Modbus Exceptions:** If the BME280 sensor is physically disconnected or damaged, the STM32 does not crash. Instead, it catches the I2C failure and safely returns a **Modbus Exception 0x04 (Slave Device Failure)** to the Modbus Client.
* **Network Conflicts:** The W5500 is configured to detect IP conflicts on the LAN and trigger a critical network error state if another device steals its static IP (192.168.0.4).

---

## Project File Structure

File location: firmware\STM32CubeIDE\STM32F411CEU6_W5500_Modbus\Core\Src or Inc.

### 1. `app.c` (Main Application Logic)
It houses the FSM and uses functions from other modules:
* `W5500_Hardware_Init()`: Configures SPI, IP address, and EXTI masks.
* `Modbus_Init()`: Opens the TCP socket on port 502.
* `BME280_Hardware_Init()`: Verifies the I2C connection and uploads calibration data.
* `BME280_Read_Measurements()`: Pulls Temp/Hum/Press data.
* `Modbus_Loop()`: Processes the network traffic when the EXTI flag is raised.

### 2. `w5500_spi.c` (Network Hardware Driver)
Acts as the bridge between the STM32 SPI peripheral and the WIZnet library. It uses **SPI Burst functions** for rapid data transfer and handles the parsing of hardware interrupt registers (`getIR()`, `getSIR()`).

### 3. `modbus_tcp.c` (Protocol Parser)
Manages the TCP socket state machine (SOCK_ESTABLISHED, SOCK_CLOSE_WAIT, SOCK_CLOSED). It dissects the 12-byte Modbus TCP frames, validates the Protocol ID, constructs responses, and handles Exception generation (e.g., Illegal Function, Illegal Data Address).

### 4. `bme280_i2c.c` (Sensor Driver)
A wrapper that connects the official BME280 library to the STM32 HAL. It translates `HAL_I2C_Mem_Read/Write` commands into a format the BME280 API understands, allowing for precise configuration of oversampling and noise filters.

---

## Python PC Client

A Python script (`read_modbus_tcp_ip.py`) is provided to act as the Modbus Master/Client.

**Features:**
* Connects to the STM32 at `192.168.0.4:502`.
* Continuously polls Holding Registers (Function 0x03).
* Decodes the integers sent by the MCU back into floats (e.g., `2354` -> `23.54 °C`).
* **Advanced Error Decoding:** Differentiates between physical network failures (Error 2, 4) and logical MCU hardware failures (Exception 0x04), ensuring precise diagnostics.

### How to run the Python client:
1. Install the required library:
   ```bash
   pip install pyModbusTCP
