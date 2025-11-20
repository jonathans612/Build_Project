#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <vector>
#include <string>
#include <cstring>
#include "minmea.h" // Include the minmea library header

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
        
        ESP_ERROR_CHECK(uart_driver_install(uart_num, 2048, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_LOGI(TAG_GPS, "GPS Reader initialized on UART %d", uart_num);
    }

    void task_loop()
    {
        std::vector<uint8_t> data(1024);
        while (true)
        {
            int len = uart_read_bytes(uart_num, data.data(), data.size() - 1, pdMS_TO_TICKS(1000));
            
            if (len > 0)
            {
                data[len] = '\0';

                // Tokenize the buffer by newline characters
                char* line = strtok((char*)data.data(), "\r\n");
                while (line != NULL)
                {
                    // Only parse valid sentences
                    if (minmea_check(line, false))
                    {
                        switch (minmea_sentence_id(line, false)) // Add 'false' for non-strict parsing
                        {
                            case MINMEA_SENTENCE_RMC: {
                                struct minmea_sentence_rmc frame;
                                if (minmea_parse_rmc(&frame, line)) {
                                    ESP_LOGI(TAG_GPS, "RMC: Lat: %f, Lon: %f, Speed: %f, Date: %02d/%02d/%02d",
                                        minmea_tocoord(&frame.latitude),
                                        minmea_tocoord(&frame.longitude),
                                        minmea_tofloat(&frame.speed),
                                        frame.date.day, frame.date.month, frame.date.year
                                    );
                                }
                            } break;

                            case MINMEA_SENTENCE_GGA: {
                                struct minmea_sentence_gga frame;
                                if (minmea_parse_gga(&frame, line)) {
                                    ESP_LOGI(TAG_GPS, "GGA: Fix Quality: %d, Sats: %d, Alt: %f",
                                        frame.fix_quality,
                                        frame.satellites_tracked,
                                        minmea_tofloat(&frame.altitude)
                                    );
                                }
                            } break;

                            case MINMEA_SENTENCE_GSV: {
                                struct minmea_sentence_gsv frame;
                                if (minmea_parse_gsv(&frame, line)) {
                                    ESP_LOGI(TAG_GPS, "GSV: Sats in view: %d (Msg %d of %d)",
                                        frame.total_sats,
                                        frame.msg_nr,
                                        frame.total_msgs
                                    );
                                    for (int i = 0; i < 4; i++) {
                                        if (frame.sats[i].nr != 0) {
                                             ESP_LOGI(TAG_GPS, "  - Sat %02d: SNR: %d dB, Elev: %d, Azim: %d",
                                                frame.sats[i].nr,
                                                frame.sats[i].snr,
                                                frame.sats[i].elevation,
                                                frame.sats[i].azimuth
                                             );
                                        }
                                    }
                                }
                            } break;
                        
                            default:
                                break;
                        }
                    }
                    // Move to the next line in the buffer
                    line = strtok(NULL, "\r\n");
                }
            }
        }
    }

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
            if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0)
            {
                is_blinking = !is_blinking;
                ESP_LOGI(TAG_LED, "Blinking is now %s", is_blinking ? "ON" : "OFF");
                if (!is_blinking) gpio_set_level(pin, 0);
            }

            while (is_blinking)
            {
                gpio_set_level(pin, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(pin, 0);
                vTaskDelay(pdMS_TO_TICKS(500));

                if (ulTaskNotifyTake(pdTRUE, 0) > 0)
                {
                    is_blinking = false;
                    ESP_LOGI(TAG_LED, "Blinking stopped");
                    gpio_set_level(pin, 0);
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
            {
                ESP_LOGI(TAG_BTN, "Button event! Sending notification.");
                if(task_to_notify != NULL) xTaskNotifyGive(task_to_notify);
            }
            last_button_state = current_button_state;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    static void start_task(void *param)
    {
        reinterpret_cast<Button *>(param)->task_loop();
    }
};

//==============================================================================
// MAIN
//==============================================================================
extern "C" void app_main(void)
{
    ESP_LOGI(TAG_APP, "Starting Application");

    static LED led(GPIO_NUM_2);
    TaskHandle_t led_task_handle;
    xTaskCreate(LED::start_task, "LED_Task", 2048, &led, 5, &led_task_handle);
    led.set_task_handle(led_task_handle);

    static Button button(GPIO_NUM_0, led.get_task_handle());
    xTaskCreate(Button::start_task, "Button_Task", 2048, &button, 10, NULL);

    static GpsReader gps(UART_NUM_2, GPIO_NUM_17, GPIO_NUM_16, 9600);
    xTaskCreate(GpsReader::start_task, "GPS_Task", 8192, &gps, 5, NULL);

    ESP_LOGI(TAG_APP, "All tasks created.");
}