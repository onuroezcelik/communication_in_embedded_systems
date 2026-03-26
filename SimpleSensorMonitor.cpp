// Simple Sensor Monitor

// 1. Include the necessary header files
#include <cstdint>
#include <thread>
#include <chrono>
#include <cstdio>
#include "I2C.h"
#include "LIN.h"


// 1. Initialize I2C controller
//   - 1.1    I2C register address 0xFF000030
//   - 1.2    Set I2C clock speed to 100kHz
//   - 1.3    Set I2C control register to host mode
//   - 1.4    Set I2C control register with bus idle low
//   - 1.5    Set I2C control register with clock on rising edge
//   - 1.6    0xFF000030: | clk speed: 2 | host: 1 |  bus_idle: 1 | clk_edge: 2 |
// 2. Initialize LIN slave
//  - 2.1    LIN register address 0xFF000040
//  - 2.2    Set LIN control register to 9600 baud rate
//  - 2.3    Set LIN control register to 1 start bit
//  - 2.4    Set LIN control register to 1 stop bit
//  - 2.5    Set LIN control register to 8 data bits
//  - 2.6    Set LIN control register to no flow control
//  - 2.7    0xFF000040: | baud rate: 8 | parity: 2 | stop bits: 2 | data bits: 2 | flow control: 2 |
// 3. Setup LIN Interrupt cb
// 4. Infinite loop reading temperature sensor data
//  - 4.1    Read 8 bit temperature sensor data at i2c address 0x20
//  - 4.2    Cache latest temperature sensor data to send over LIN
//  - 4.3    Send temperature sensor data over LIN when requested

#define I2C_HW_ADDR 0xFF000030
#define LIN_HW_ADDR 0xFF000040

#define ADC_ADDR 0x20
#define ADC_REG  0x00
#define SLEEP_DURATION_MS 1000
#define SAMPLE_COUNT 10

// Reading the contents of the LIN Descriptor File is too much for this project
static uint8_t g_current_val = 0;
static uint8_t g_samples[SAMPLE_COUNT] = {0};
static uint8_t g_sample_index = 0;

static uint8_t calculate_average() {
    uint16_t sum = 0;

    for (uint8_t i = 0; i < SAMPLE_COUNT; i++) {
        sum += g_samples[i];
    }

    return static_cast<uint8_t>(sum / SAMPLE_COUNT);
}

void lin_rx_isr(uint8_t id) {

    // If the LIN master is looking for node temp id, send the avg temp over LIN
    if(id == LIN_AVG_TEMP_SENSOR_ID) {
        // calculate the avg;
        uint8_t avg_temp = calculate_average();
        printf("LIN avg temp request received, sending: %d\n", avg_temp);
        lin_write_response_data(LIN_AVG_TEMP_SENSOR_ID, &avg_temp, 1);


    } else if(id == LIN_CURRENT_TEMP_SENSOR_ID) {
        // If the LIN master is looking for node temp id, send the last sample over LIN
        printf("LIN current temp request received, sending: %d\n", g_current_val);
        lin_write_response_data(LIN_CURRENT_TEMP_SENSOR_ID, &g_current_val, 1);
    }

    // Clear the lin interrupt before exit isr:
    lin_clear_rx_frame_interrupt();
}

int main(int argc, char **argv) {

    // Configure the I2C controller
    uint32_t i2c_config =
        I2C_CLK_100KHZ |
        I2C_HOST |
        I2C_IDLE_LOW |
        I2C_CLK_RISING_EDGE;

    // Apply configuration
    i2c_write_config(I2C_HW_ADDR, i2c_config);

    uint32_t lin_config =
        LIN_BAUD_RATE_9600 |
        LIN_START_BITS_1 |
        LIN_STOP_BITS_1 |
        LIN_DATA_BITS_8 |
        LIN_PARITY_NONE |
        LIN_NO_FLOW_CONTROL |
        LIN_MODE_FOLLOWER;

    lin_write_config(LIN_HW_ADDR, lin_config);

    // Add the LIN RX ISR
    lin_add_rx_frame_header_interrupt(lin_rx_isr);

    while(true) {

        uint8_t temp_val = 0;
        uint8_t reg = ADC_REG;

        // Select ADC register
        i2c_write_data(ADC_ADDR, &reg, 1);

        // Read 8-bit temperature sensor data
        i2c_read_data(ADC_ADDR, &temp_val, 1);

        // Cache latest temperature sensor data
        g_current_val = temp_val;

        // Store sample for average calculation
        g_samples[g_sample_index] = temp_val;
        g_sample_index = (g_sample_index + 1) % SAMPLE_COUNT;

        printf("ADC Value: %d\n", g_current_val);

        std::this_thread::sleep_for(std::chrono::milliseconds (SLEEP_DURATION_MS));
    }
}
