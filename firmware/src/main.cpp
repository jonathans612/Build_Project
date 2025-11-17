#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_GPIO    GPIO_NUM_2
#define BUTTON_GPIO GPIO_NUM_0

static const char *TAG = "TOGGLE_BLINK";

// --- Global Task Handle ---
// We need a handle to the blink_task so the button_task can send it a notification.
TaskHandle_t blink_task_handle = NULL;

// --- The Blink Task ---
void blink_task(void *pvParameter)
{
    bool is_blinking = false; // Internal state for the blinker
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (true) {
        // ulTaskNotifyTake() is a special function that waits for a notification.
        // The first parameter (pdTRUE) clears the notification count on exit.
        // The second parameter (portMAX_DELAY) means it will wait forever.
        // This is the "pause" point. The code will block here until notified.
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {
            // We received a notification! Toggle the blinking state.
            is_blinking = !is_blinking;
            ESP_LOGI(TAG, "Notification received! Blinking is now %s", is_blinking ? "ON" : "OFF");
        }

        // If blinking is enabled, enter the blink loop.
        while (is_blinking) {
            gpio_set_level(LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // --- IMPORTANT ---
            // Check for another notification *while blinking*.
            // This allows us to receive a "stop" command even while in the blink loop.
            // The second parameter (0) means it's a non-blocking check. It returns instantly.
            if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
                is_blinking = !is_blinking; // Toggle state to OFF
                ESP_LOGI(TAG, "Notification received! Blinking is now %s", is_blinking ? "ON" : "OFF");
                gpio_set_level(LED_GPIO, 0); // Ensure LED is off before exiting
            }
        }
    }
}

// --- The Button-Watching Task ---
void button_task(void *pvParameter)
{
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
    bool last_button_state = 1; // Assume button is initially not pressed (HIGH)

    while (true) {
        bool current_button_state = gpio_get_level(BUTTON_GPIO);
        
        // Check for a "falling edge" - from not pressed (1) to pressed (0).
        if (last_button_state == 1 && current_button_state == 0) {
            ESP_LOGI(TAG, "Button pressed! Sending notification.");
            // Send a notification to the blink_task.
            xTaskNotifyGive(blink_task_handle);
        }
        
        last_button_state = current_button_state;

        // Debounce delay - wait a short moment to avoid multiple triggers from one press.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting up. Creating tasks.");

    // Create the blink task and store its handle.
    xTaskCreate(blink_task, "Blink Task", 2048, NULL, 5, &blink_task_handle);

    // Create the button-watching task.
    xTaskCreate(button_task, "Button Task", 2048, NULL, 10, NULL); // Higher priority
}