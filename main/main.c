#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"

// Define the size of the UART reception buffer
static const int RX_BUFFER_SIZE = 2024;

// --- UART0 (Console Output) Pins ---
// Note: ESP32 UART0 is typically used for console and flashing, but the video redefines pins for a custom setup.
// We will use standard UART0 for console logging. The video defines these pins:
#define TXT_PIN GPIO_NUM_1 
#define RXT_PIN GPIO_NUM_3 

// --- UART2 (GPS Module) Pins ---
// These are the GPIO pins connected to the GT-U7 module
#define TXT_PIN_2 GPIO_NUM_17
#define RXT_PIN_2 GPIO_NUM_16

// Global Tag for ESP logging
static const char *TAG = "GPS_DRIVER";

// Forward declarations for LCD (commented out as the LCD header is missing)
// void LCD_init(void);
// void LCD_command(unsigned char);
// void LCD_string(unsigned char*);

// Array of strings used in the video (for LCD messages)
char ARR_1[] = "LAT ";
char ARR_2[] = "LONG";
char ARR_3[] = "           "; // 11 spaces for clearing first row
char ARR_4[] = "            "; // 12 spaces for clearing second row

/**
 * @brief Function to parse and display latitude and longitude from the GPGGA string
 * * @param str The raw $GPGGA NMEA sentence string
 */
static void get_string(char *str) {
    int i = 0, count = 0, k = 0;
    int n, m; // Unused in the final parsing logic, but defined in video

    // New string to hold the coordinate data temporarily
    char new_str[50]; 
    
    // 1. Find the coordinates after the second comma (index 2)
    // The GPGGA format is: $GPGGA,Time,Lat,N/S,Long,E/W,...
    // The first two fields are $GPGGA and Time, so Lat is the third field (after the 2nd comma)
    while (count < 2) { 
        if (str[i] == ',') {
            count++;
        }
        i++;
    }

    // 2. Extract the Latitude and Longitude string 
    // The video extracts 24 characters starting from the first digit of Latitude.
    // This includes Latitude, N/S, Longitude, E/W
    for (int j = i; j < i + 24; j++) {
        new_str[k++] = str[j];
    }
    new_str[k] = '\0'; // Null terminate the string
    
    // --- Console Output of Coordinates ---
    ESP_LOGI(TAG, "Coordinates: %s", new_str);
    
    // --- The original video logic for LCD commands are placed here: ---
    
    // LCD_command(0x84); // Move cursor to Row 1, Column 4
    // int e = 0;
    // while(new_str[e] != 'N') // Print Latitude until N/S indicator
    // {
    //     LCD_data(new_str[e]);
    //     e++;
    // }
    
    // LCD_command(0xC4); // Move cursor to Row 2, Column 4
    // e = e + 2; // Move past 'N' and the comma to Longitude
    // while(new_str[e] != 'E') // Print Longitude until E/W indicator
    // {
    //     LCD_data(new_str[e]);
    //     e++;
    // }
    
    // --- End of LCD logic ---
}

/**
 * @brief Initialize UART0 for console logging
 */
void uart_init_0(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    // Install UART driver, and set communication parameters
    uart_driver_install(UART_NUM_0, RX_BUFFER_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    
    // Set UART pins (TX: 1, RX: 3)
    uart_set_pin(UART_NUM_0, TXT_PIN, RXT_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

/**
 * @brief Initialize UART2 for GPS module communication
 */
void uart_init_2(void) {
    const uart_config_t uart_config_2 = {
        .baud_rate = 9600, // GT-U7 Default Baud Rate
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    // Install UART driver
    uart_driver_install(UART_NUM_2, RX_BUFFER_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_config_2);
    
    // Set UART pins (TX: 17, RX: 16)
    uart_set_pin(UART_NUM_2, TXT_PIN_2, RXT_PIN_2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

/**
 * @brief FreeRTOS Task to read data from the GPS module (UART2)
 */
#define RX_TASK_TAG "GPS_RECEIVER" // Define a log tag for the task

static void rx_task(void *arg) {
    // Dynamically allocate memory for the received data buffer
    uint8_t *data = (uint8_t *)malloc(RX_BUFFER_SIZE + 1);
    
    // Dynamically allocate memory for string extraction
    char *str_2 = (char *)malloc(RX_BUFFER_SIZE + 1); 

    if (data == NULL || str_2 == NULL) {
        ESP_LOGE(RX_TASK_TAG, "Failed to allocate memory for RX buffers!");
        vTaskDelete(NULL); // Stop the task if memory allocation failed
    }
    
    // Log startup to confirm the task is running
    ESP_LOGI(RX_TASK_TAG, "UART RX task started successfully."); 

    while (1) {
        // Read data from the UART2 ring buffer (GPS Module)
        int rx_bytes = uart_read_bytes(UART_NUM_2, data, RX_BUFFER_SIZE, 1000 / portTICK_PERIOD_MS);

        if (rx_bytes > 0) {
            data[rx_bytes] = '\0'; // Null terminate the received buffer

            // --- Log Raw NMEA Data to Console (UART0) ---
            // Use ESP_LOGI for better formatting and log control
            ESP_LOGI(RX_TASK_TAG, "Raw Data: %s", (char *)data);
            
            // --- NMEA Sentence Extraction Logic ---
            const char *needle = "$GPGGA";
            char *p = NULL;
            
            // Find the $GPGGA sentence in the raw data buffer
            p = strstr((const char *)data, needle);

            if (p) {
                int j = 0;
                // Copy the NMEA sentence character by character until the newline
                // Added check for buffer size (j < RX_BUFFER_SIZE) to prevent overflow
                for (int i = 0; p[i] != '\n' && p[i] != '\0' && j < RX_BUFFER_SIZE; i++) {
                    str_2[j++] = p[i];
                }
                
                // --- FIXED NULL TERMINATION ---
                if (j < RX_BUFFER_SIZE) {
                    str_2[j++] = '\n'; // Add newline
                }
                str_2[j] = '\0';   // Null terminate the extracted string
                
                // Call the function to parse and log the coordinates
                get_string((char *)str_2);
            }
            
            // Removed delay here as uart_read_bytes already has a 1000ms timeout
            // If you want a smaller polling rate, reduce the timeout in uart_read_bytes.
        }
    }
    // Note: The memory free calls remain unreachable because of while(1)
}

/**
 * @brief Application entry point
 */
void app_main(void) {
    // 1. Initialize LCD (The video shows an external component for this)
    // LCD_init();

    // 2. Initialize UARTs
    uart_init_0(); // For Console Log
    uart_init_2(); // For GPS Module

    // 3. Create the FreeRTOS task to handle GPS data
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, 5, NULL);
    
    // --- LCD Display Initial Messages (Commented out) ---
    // LCD_command(0x80); 
    // LCD_string((unsigned char *)ARR_1); // Print "LAT"
    // LCD_command(0xC0); 
    // LCD_string((unsigned char *)ARR_2); // Print "LONG"
}