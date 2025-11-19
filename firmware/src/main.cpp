#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <vector> // For std::vector (RAII)

// --- TAGs for logging ---
static const char *TAG_APP = "APP";
static const char *TAG_GPS = "GPS";
static const char *TAG_BTN = "BTN";
static const char *TAG_LED = "LED";

//==============================================================================
// GPS READER CLASS
//==============================================================================
class GpsReader
{
private:
    const uart_port_t uart_num;
    const int tx_pin;
    const int rx_pin;
    const int baud_rate;

    // Helper function to print data as hex
    void print_buffer_hex(const uint8_t* data, int len)
    {
        char hex_buffer[len * 3 + 1];
        for (int i = 0; i < len; i++) {
            sprintf(hex_buffer + i * 3, "%02X ", data[i]);
        }
        hex_buffer[len * 3] = '\0';
        ESP_LOGI(TAG_GPS, "HEX DATA: [ %s]", hex_buffer);
    }

public:
    GpsReader(uart_port_t port, int tx, int rx, int baud)
        : uart_num(port), tx_pin(tx), rx_pin(rx), baud_rate(baud)
    {
        uart_config_t uart_config = {
            .baud_rate = baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        
        // Install UART driver, and get the queue.
        ESP_ERROR_CHECK(uart_driver_install(uart_num, 2048, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
        
        // Set UART pins (using configured pins)
        ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_LOGI(TAG_GPS, "GPS Reader initialized on UART %d", uart_num);
    }

    void task_loop()
    {
        std::vector<uint8_t> data(1024);
        while (true)
        {
            int len = uart_read_bytes(uart_num, data.data(), data.size(), pdMS_TO_TICKS(1000));
            if (len > 0)
            {
                ESP_LOGI(TAG_GPS, "Received %d bytes. Attempting to print as string:", len);
                // Print as a string (what you have now)
                printf("%.*s\n", len, data.data());

                // Also print as HEX for debugging
                print_buffer_hex(data.data(), len);
            }
        }
    }

    // Static function to launch the task
    static void start_task(void *param)
    {
        reinterpret_cast<GpsReader *>(param)->task_loop();
    }
};

//==============================================================================
// LED CLASS
//==============================================================================
class LED
{
private:
    const gpio_num_t pin;
    bool is_blinking = false;
    TaskHandle_t task_handle = NULL;

public:
    LED(gpio_num_t gpio_pin) : pin(gpio_pin)
    {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        ESP_LOGI(TAG_LED, "LED initialized on GPIO %d", pin);
    }

    TaskHandle_t get_task_handle() const { return task_handle; }

    void set_task_handle(TaskHandle_t handle) { task_handle = handle; }

    void task_loop()
    {
        while (true)
        {
            // Wait for a notification to toggle state
            if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0)
            {
                is_blinking = !is_blinking;
                ESP_LOGI(TAG_LED, "Blinking is now %s", is_blinking ? "ON" : "OFF");
                if (!is_blinking) gpio_set_level(pin, 0); // Turn off LED if stopping
            }

            // Inner blink loop
            while (is_blinking)
            {
                gpio_set_level(pin, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(pin, 0);
                vTaskDelay(pdMS_TO_TICKS(500));

                // Check for notification to stop blinking (non-blocking)
                if (ulTaskNotifyTake(pdTRUE, 0) > 0)
                {
                    is_blinking = false;
                    ESP_LOGI(TAG_LED, "Blinking stopped");
                    gpio_set_level(pin, 0); // Ensure LED is off
                }
            }
        }
    }

    static void start_task(void *param)
    {
        reinterpret_cast<LED *>(param)->task_loop();
    }
};

//==============================================================================
// BUTTON CLASS
//==============================================================================
class Button
{
private:
    const gpio_num_t pin;
    TaskHandle_t task_to_notify;

public:
    Button(gpio_num_t gpio_pin, TaskHandle_t notify_handle) : pin(gpio_pin), task_to_notify(notify_handle)
    {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
        ESP_LOGI(TAG_BTN, "Button initialized on GPIO %d", pin);
    }

    void task_loop()
    {
        bool last_button_state = (gpio_get_level(pin) == 1);
        while (true)
        {
            bool current_button_state = (gpio_get_level(pin) == 1);
            if (!last_button_state && current_button_state)
            { // Rising edge on pull-up (release)
                // Or use `if (last_button_state && !current_button_state)` for press
                ESP_LOGI(TAG_BTN, "Button event! Sending notification.");
                xTaskNotifyGive(task_to_notify);
            }
            last_button_state = current_button_state;
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        }
    }

    static void start_task(void *param)
    {
        reinterpret_cast<Button *>(param)->task_loop();
    }
};


//==============================================================================
// MAIN APPLICATION
//==============================================================================
extern "C" void app_main(void)
{
    ESP_LOGI(TAG_APP, "Starting Application");

    // --- Create LED and its Task ---
    // We need to create the LED object first to get its task handle
    static LED led(GPIO_NUM_2);
    TaskHandle_t led_task_handle;
    xTaskCreate(LED::start_task, "LED_Task", 2048, &led, 5, &led_task_handle);
    led.set_task_handle(led_task_handle); // Store handle inside the object

    // --- Create Button and its Task ---
    // The button needs the LED's task handle to send notifications
    static Button button(GPIO_NUM_0, led.get_task_handle());
    xTaskCreate(Button::start_task, "Button_Task", 2048, &button, 10, NULL);

    // --- Create GPS and its Task ---
    static GpsReader gps(UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16, 9600);
    xTaskCreate(GpsReader::start_task, "GPS_Task", 4096, &gps, 5, NULL);

    ESP_LOGI(TAG_APP, "All tasks created.");
}