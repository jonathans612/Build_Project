#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

// --- Configuration Defines for Adafruit ESP32-S3 Metro ---
#define UART_NUM                UART_NUM_2
#define TXD_PIN                 (GPIO_NUM_17)   // UART2 TX pin on ESP32-S3 (Connected to Sensor RX)
#define RXD_PIN                 (GPIO_NUM_18)   // UART2 RX pin on ESP32-S3 (Connected to Sensor TX)
#define BUF_SIZE                (128)
#define BAUD_RATE               115200          // Confirmed from sensor spec

// --- Sensor Protocol Defines ---
#define COMMAND_POLL            0x55            // Command to request data
#define PACKET_LENGTH           4               // Expected packet size (0xFF, H_DATA, L_DATA, SUM)
// Correction factor: 10 (mm to cm) * 4.37 (SoS Water/Air Mismatch) = 43.7
#define AIR_CORRECTION_FACTOR   43.7f           // Float value for accurate division

static const char *TAG = "UART_SENSOR";

// Function to handle the sensor communication
void sensor_task(void *arg) {
    uint8_t tx_command[1] = {COMMAND_POLL};
    uint8_t buffer_RTT[PACKET_LENGTH] = {0};
    uint8_t CS = 0;
    int Distance_Raw_MM = 0;      // Holds the raw 16-bit value in mm (inflated by SoS)
    float Distance_Corrected_CM = 0.0f; // Holds the final, corrected distance in cm
    int len = 0;

    ESP_LOGI(TAG, "TASK STARTED SUCCESSFULLY. Beginning UART setup...");

    // Configure UART parameters
    const uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver
    ESP_ERROR_CHECK(uart_driver_install(
        UART_NUM,
        BUF_SIZE * 2,
        BUF_SIZE * 2,
        0,
        NULL,
        ESP_INTR_FLAG_IRAM
    ));

    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART SETUP COMPLETE. Starting polling loop.");

    // Main polling loop
    while (1) {
        // 1. Send Command (0x55)
        uart_write_bytes(UART_NUM, (const char *)tx_command, 1);

        // 2. Wait for the sensor to generate the data packet (0xFF...)
        // Give the sensor generous time (150ms) to measure and begin transmission.
        vTaskDelay(pdMS_TO_TICKS(150)); 
        
        // 3. Read the Response
        // Read the full expected packet length (4 bytes) with a long timeout (500ms)
        len = uart_read_bytes(
            UART_NUM, 
            buffer_RTT, 
            PACKET_LENGTH, 
            pdMS_TO_TICKS(500) 
        );

        // 4. Process the Data
        if (len == PACKET_LENGTH && buffer_RTT[0] == 0xFF) {
            
            // Raw value (in mm, inflated by SoS mismatch)
            Distance_Raw_MM = (buffer_RTT[1] << 8) + buffer_RTT[2]; 

            // Calculate Checksum
            CS = (buffer_RTT[0] + buffer_RTT[1] + buffer_RTT[2]) & 0xFF; 

            // Checksum verification
            if (buffer_RTT[3] == CS) {
                
                // FINAL CORRECTION:
                // Raw mm / 43.7 = Corrected distance in cm
                Distance_Corrected_CM = (float)Distance_Raw_MM / AIR_CORRECTION_FACTOR;
                
                // Print the result to console
                ESP_LOGI(TAG, "Distance: %.1f cm (Raw Value: %d mm)", 
                    Distance_Corrected_CM, 
                    Distance_Raw_MM
                ); 

            } else {
                ESP_LOGE(TAG, "Checksum failed: Calculated 0x%02X, Received 0x%02X", CS, buffer_RTT[3]);
            }
            
        } else if (len > 0) {
            // Received partial or garbage data. Clear it out and warn.
            ESP_LOGW(TAG, "Partial/Bad data received (%d bytes). Start: 0x%02X. Flushing buffer.", len, buffer_RTT[0]);
            uart_flush(UART_NUM);
        } else {
             // len == 0 (Timeout occurred)
             ESP_LOGW(TAG, "Read timeout: No data received.");
        }

        // Wait before next poll cycle
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Main ESP-IDF function
void app_main(void) {
    // Start the sensor communication task.
    BaseType_t result = xTaskCreate(sensor_task, "uart_sensor_task", 4096, NULL, 10, NULL);

    if (result != pdPASS) {
        ESP_LOGE("MAIN", "Failed to create sensor_task! Out of memory.");
    }
}