# Simple Vehicle Communication Project 

This project simulates a simplified in-vehicle communication architecture using three independent modules interacting over I2C, LIN, CAN, and SPI.

---

## Architecture Overview

The system consists of three modules:

### 1. SimpleTemperatureMonitor
- Reads temperature from a sensor via I2C
- Stores recent samples to compute an average temperature
- Responds to LIN master requests:
  - Current temperature
  - Average temperature

---

### 2. BatteryTemperatureVehicleModule
- Acts as LIN master
- Periodically requests:
  - Current temperature
  - Average temperature
- Stores received values internally
- Responds to CAN RTR (Remote Transmission Request) frames with:
  - Current temperature
  - Average temperature
  - Current timestamp (seconds)

---

### 3. DataLoggingVehicleModule
- Sends CAN RTR requests to the Battery module
- Receives CAN DATA frames containing:
  - Current temperature
  - Average temperature
  - Timestamp
- Buffers received samples
- Writes data to SPI flash (CS1) in pages

---

## Communication Flow
I2C → LIN → CAN → SPI

SimpleTemperatureMonitor
→ (LIN)
BatteryTemperatureVehicleModule
→ (CAN)
DataLoggingVehicleModule
→ (SPI Flash)

---

## Getting Started

### Build the project

```
bash
mkdir build
cd build
cmake ..
make
```

### Running the System

Run each module in a separate terminal:

```
./SimpleSensorMonitor
./BatteryTemperatureVehicleModule
./DataLoggingVehicleModule
```

### Dependencies
- CMake
- C++17 compiler

## Testing

System behavior was validated by:

Verifying I2C temperature reads
Observing LIN communication between sensor and battery module
Confirming CAN request/response behavior
Validating SPI flash page writes

Example output from DataLoggingVehicleModule:

Average temperature data: 59
Current temperature data: 60
Current time data: 1774526781
Wrote SPI flash page: 0

## Project Instructions

### SimpleTemperatureMonitor
#### Configured the I2C controller
### I2C Configuration

The I2C controller is initialized using the hardware register at address `0xFF000030`.

The following configuration is applied:
- Clock speed: 100 kHz
- Mode: Host
- Bus idle state: Low
- Clock edge: Rising

These settings are combined into the I2C configuration register before communication with the temperature sensor begins.

```
#define I2C_HW_ADDR 0xFF000030

    // Configure the I2C controller
    uint32_t i2c_config =
        I2C_CLK_100KHZ |
        I2C_HOST |
        I2C_IDLE_LOW |
        I2C_CLK_RISING_EDGE;

    // Apply configuration
    i2c_write_config(I2C_HW_ADDR, i2c_config);
````

- Implemented periodic temperature reading from the sensor
- Stored recent samples to compute average temperature
- Responded to LIN requests with:
  - Current temperature
  - Average temperature

### BatteryTemperatureVehicleModule
- Configured LIN as master
- Periodically requested temperature data from the sensor module
- Stored received temperature values
- Implemented CAN ISR to respond to RTR frames with:
  - Current temperature
  - Average temperature
  - Current timestamp

### DataLoggingVehicleModule
- Configured CAN and SPI controllers
- Sent periodic CAN RTR requests
- Implemented CAN ISR to receive data frames
- Buffered received samples (temperature + timestamp)
- Wrote buffered data to SPI flash in page-sized chunks

## Built With

* C++17 - Core programming language
* CMake - Build system
* Custom hardware simulation layer (I2C, LIN, CAN, SPI)

## License
This project is part of a Udacity course and is intended for educational purposes only.
