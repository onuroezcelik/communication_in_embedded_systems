#include "SPI.h"
#include "CAN.h"
#include <cstring>
#include <thread>
#include <cstdio>

// 1. Configure the SPI controller
// --- 1.1 SPI clock 1MHz, CS1
// 2. Configure the CAN controller
// --- 2.1 Baud 100K, 11 bit format
// 3. Add the CAN RX ISR
// 4. Every 500ms send the CAN RTR frames to the CAN Bus
// --- 4.5 CAN_AVG_TEMPERATURE_11_SENSOR_ID
// --- 4.6 CAN_CURRENT_TEMP_11_SENSOR_ID
// --- 4.7 CAN_TIME_11_SENSOR_ID
// 5. Save the temperature results to the SPI Flash (See SPI Flash section below)

// SPI Flash Brief
// * The SPI Flash is on CS1, not CS0
// * The SPI Flash has a Page size of of 64 bytes defined as SPI_FLASH_PAGE_SIZE in SPI.h
// * The SPI Flash has a total size of 4096 bytes defined as SPI_FLASH_SZ in SPI.h
// * A SPI Flash must be written to in pages, not bytes
// * A SPI Flash will return an error if you attemp to write to a page that is not erased
// * The SPI Flash expects the MOSI data to look like the following
//               | Flash Command | Page Number | Data 1 | Data 2 | ... | Data 64 |
// * The available flash commands area (SPI.h):
// ** SPI_FLASH_CMD_WRITE
// ** SPI_FLASH_CMD_READ
// ** SPI_FLASH_CMD_ERASE



#define SPI_FLASH_DATA_PTS_PER_PAGE  (SPI_FLASH_PAGE_SIZE / sizeof(SPI_FLASH_data_pt_t))
#define SPI_FLASH_PAGES_TOTAL        (SPI_FLASH_SZ / SPI_FLASH_PAGE_SIZE)

static SPI_FLASH_data_pt_t g_page_buffer[SPI_FLASH_DATA_PTS_PER_PAGE];
static uint8_t g_page_buffer_count = 0;

static uint8_t g_pending_avg_temp = 0;
static uint8_t g_pending_current_temp = 0;
static unsigned int g_pending_time = 0;

static bool g_has_avg_temp = false;
static bool g_has_current_temp = false;
static bool g_has_time = false;

static uint8_t g_current_flash_page = 0;

static void try_commit_sample() {
    if (g_has_avg_temp && g_has_current_temp && g_has_time) {
        if (g_page_buffer_count < SPI_FLASH_DATA_PTS_PER_PAGE) {
            g_page_buffer[g_page_buffer_count].avg_temp = g_pending_avg_temp;
            g_page_buffer[g_page_buffer_count].current_temp = g_pending_current_temp;
            g_page_buffer[g_page_buffer_count].time = g_pending_time;
            g_page_buffer_count++;
        }

        g_has_avg_temp = false;
        g_has_current_temp = false;
        g_has_time = false;
    }
}

void can_packet_isr(uint32_t id, CAN_FRAME_TYPES type, uint8_t *data, uint8_t len) {
    // we can not save the page in the isr, too slow
    // so you need to accomodate for this in the main loop

    // Do something with the CAN packet
    if (type == CAN_DATA_FRAME) {
        if (id == CAN_AVG_TEMPERATURE_11_SENSOR_ID && len >= 1) {
            g_pending_avg_temp = data[0];
            g_has_avg_temp = true;

            printf("Average temperature data: %d\n", data[0]);
            try_commit_sample();

        } else if (id == CAN_CURRENT_TEMP_11_SENSOR_ID && len >= 1) {
            g_pending_current_temp = data[0];
            g_has_current_temp = true;

            printf("Current temperature data: %d\n", data[0]);
            try_commit_sample();

        } else if (id == CAN_TIME_11_SENSOR_ID && len >= 4) {
            g_pending_time =
                static_cast<unsigned int>(data[0]) |
                (static_cast<unsigned int>(data[1]) << 8) |
                (static_cast<unsigned int>(data[2]) << 16) |
                (static_cast<unsigned int>(data[3]) << 24);

            g_has_time = true;

            printf("Current time data: %u\n", g_pending_time);
            try_commit_sample();

        } else {
            printf("Unknown CAN packet received\n");
        }
    } else {
        printf("Unknown CAN packet received\n");
    }


    // Clear the can interrupt before exit isr:
    can_clear_rx_packet_interrupt();
}

static void write_page_to_spi_flash() {
    if (g_page_buffer_count < SPI_FLASH_DATA_PTS_PER_PAGE) {
        return;
    }

    if (g_current_flash_page >= SPI_FLASH_PAGES_TOTAL) {
        printf("SPI Flash is full\n");
        return;
    }

    uint8_t erase_buf[2];
    erase_buf[0] = SPI_FLASH_CMD_ERASE;
    erase_buf[1] = g_current_flash_page;

    SPI_xmit_t erase_xmit;
    erase_xmit.cmd = SPI_FLASH_CMD_ERASE;
    erase_xmit.len = 2;
    erase_xmit.data = erase_buf;

    spi_write_data(&erase_xmit, SPI_CS_1);

    uint8_t write_buf[2 + SPI_FLASH_PAGE_SIZE];
    write_buf[0] = SPI_FLASH_CMD_WRITE;
    write_buf[1] = g_current_flash_page;

    std::memcpy(&write_buf[2], g_page_buffer, SPI_FLASH_PAGE_SIZE);

    SPI_xmit_t write_xmit;
    write_xmit.cmd = SPI_FLASH_CMD_WRITE;
    write_xmit.len = 2 + SPI_FLASH_PAGE_SIZE;
    write_xmit.data = write_buf;

    spi_write_data(&write_xmit, SPI_CS_1);

    printf("Wrote SPI flash page: %d\n", g_current_flash_page);

    g_current_flash_page++;
    g_page_buffer_count = 0;
}


#define SLEEP_NOW_MS(ms)   std::this_thread::sleep_for(std::chrono::milliseconds(ms))

int main(int argc, char **argv) {
    // Configure the SPI and CAN controllers;
    
    uint32_t spi_config =
        SPI_CLK_1MHZ |
        SPI_CS_1 ;

    spi_write_config(SPI_HARDWARE_REGISTER, spi_config);

    uint32_t can_config =
        CAN_BAUD_RATE_100K |
        CAN_FORMAT_11BIT;

    can_write_config(CAN_HARDWARE_REGISTER, can_config);

    can_add_filter(0, 0x7FF, CAN_AVG_TEMPERATURE_11_SENSOR_ID);
    can_add_filter(1, 0x7FF, CAN_CURRENT_TEMP_11_SENSOR_ID);
    can_add_filter(2, 0x7FF, CAN_TIME_11_SENSOR_ID);

    // Add the CAN RX ISR
    can_add_rx_packet_interrupt(can_packet_isr);

    while(true) {
        // Send the CAN RTR frames to the BatteryTemperatureVehicleModule every 500ms
        // Once a full SPI Flash page size worth of data is received, save it to the SPI Flash
        can_send_new_packet(CAN_AVG_TEMPERATURE_11_SENSOR_ID, CAN_RTR_FRAME, nullptr, 0);
        can_send_new_packet(CAN_CURRENT_TEMP_11_SENSOR_ID, CAN_RTR_FRAME, nullptr, 0);
        can_send_new_packet(CAN_TIME_11_SENSOR_ID, CAN_RTR_FRAME, nullptr, 0);

        if (g_page_buffer_count >= SPI_FLASH_DATA_PTS_PER_PAGE) {
            write_page_to_spi_flash();
        }

        SLEEP_NOW_MS(500);

    }
    return 0;
}
