#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h" // For WiFi events
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_wifi.h"              // For WiFi
#include "nvs_flash.h"             // For WiFi persistent storage
#include "mqtt_client.h"           // For MQTT
#include <vector>
#include <string>
#include <cstring>
#include <sstream>                 // For building strings
#include "minmea.h"

// --- WiFi and MQTT Configuration ---
// !!! IMPORTANT: REPLACE with your details !!!
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER_URL "mqtt://YOUR_MQTT_BROKER_IP_OR_URL" // e.g., "mqtt://192.168.1.100:1883"

// --- TAGs for logging ---
static const char *TAG_APP = "APP";
static const char *TAG_GPS = "GPS";
static const char *TAG_BTN = "BTN";
static const char *TAG_LED = "LED";
static const char *TAG_MQTT = "MQTT";

// Forward declaration for cross-class communication
class MqttClient;

// Global pointer to the MQTT client for the GPS task to use
MqttClient* g_mqtt_client = nullptr;

//==============================================================================
// MQTT CLIENT CLASS
//==============================================================================
class MqttClient
{
private:
    esp_mqtt_client_handle_t client = NULL;
    EventGroupHandle_t wifi_event_group;
    const int WIFI_CONNECTED_BIT = BIT0;
    bool is_connected = false;

    static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
        MqttClient* mqtt_instance = reinterpret_cast<MqttClient*>(arg);
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG_MQTT, "Disconnected from WiFi, retrying...");
            esp_wifi_connect();
            xEventGroupClearBits(mqtt_instance->wifi_event_group, mqtt_instance->WIFI_CONNECTED_BIT);
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG_MQTT, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(mqtt_instance->wifi_event_group, mqtt_instance->WIFI_CONNECTED_BIT);
        }
    }

    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
        MqttClient* mqtt_instance = reinterpret_cast<MqttClient*>(handler_args);
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
        switch ((esp_mqtt_event_id_t)event_id) {
            case MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
                mqtt_instance->is_connected = true;
                break;
            case MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
                mqtt_instance->is_connected = false;
                break;
            case MQTT_EVENT_ERROR:
                ESP_LOGE(TAG_MQTT, "MQTT_EVENT_ERROR");
                break;
            default:
                break;
        }
    }

public:
    MqttClient() {
        wifi_event_group = xEventGroupCreate();
    }

    void connect() {
        // 1. Initialize NVS (required for WiFi)
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        // 2. Initialize WiFi
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        // 3. Register event handlers
        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, this, &instance_got_ip));

        // 4. Configure and start WiFi
        wifi_config_t wifi_config = {};
        strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
        strcpy((char*)wifi_config.sta.password, WIFI_PASS);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG_MQTT, "WiFi initialization finished. Waiting for connection...");
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG_MQTT, "Connected to WiFi.");
        
        // 5. Configure and start MQTT
        esp_mqtt_client_config_t mqtt_cfg = {};
        mqtt_cfg.broker.address.uri = MQTT_BROKER_URL;

        mqtt_cfg.credentials.username = "YOUR_USERNAME";  // replace with the username you set using mosquitto_passwd
        mqtt_cfg.credentials.authentication.password = "YOUR_PASSWORD";  // replace with the password you set using mosquitto_passwd
        
        client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, this);
        esp_mqtt_client_start(client);
        ESP_LOGI(TAG_MQTT, "MQTT client started.");
    }

    void publish(const std::string& topic, const std::string& payload) {
        if (is_connected) {
            esp_mqtt_client_publish(client, topic.c_str(), payload.c_str(), 0, 1, 0);
        }
    }
};

//==============================================================================
// GPS READER CLASS (Updated)
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
                                if (minmea_parse_rmc(&frame, line) && frame.valid) {
                                    // Use ostringstream for easy float-to-string conversion
                                    std::ostringstream payload;
                                    payload << "{\"lat\":" << minmea_tocoord(&frame.latitude)
                                            << ",\"lon\":" << minmea_tocoord(&frame.longitude)
                                            << ",\"speed\":" << minmea_tofloat(&frame.speed)
                                            << "}";
                                    if(g_mqtt_client) g_mqtt_client->publish("gps/location", payload.str());
                                }
                            } break;

                            case MINMEA_SENTENCE_GGA: {
                                struct minmea_sentence_gga frame;
                                if (minmea_parse_gga(&frame, line)) {
                                    std::ostringstream payload;
                                    payload << "{\"quality\":" << frame.fix_quality
                                            << ",\"satellites\":" << frame.satellites_tracked
                                            << ",\"altitude\":" << minmea_tofloat(&frame.altitude)
                                            << "}";
                                    if(g_mqtt_client) g_mqtt_client->publish("gps/stats", payload.str());
                                }
                            } break;

                            // case MINMEA_SENTENCE_GSV: {
                            //     struct minmea_sentence_gsv frame;
                            //     if (minmea_parse_gsv(&frame, line)) {
                            //         // Publish to a separate "diagnostic" topic
                            //         std::ostringstream payload;
                            //         payload << "{\"total_sats\":" << frame.total_sats
                            //                 << ",\"msg_nr\":" << frame.msg_nr
                            //                 << ",\"total_msgs\":" << frame.total_msgs
                            //                 << "}";
                            //         if(g_mqtt_client) g_mqtt_client->publish("gps/diag/sats_in_view", payload.str());
                            //     }
                            // } break;
                        
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

    // --- Create MQTT and Connect to WiFi ---
    // This is now the FIRST step, as we need network for other things
    static MqttClient mqtt_client;
    g_mqtt_client = &mqtt_client; // Make it globally accessible
    mqtt_client.connect(); // This function blocks until WiFi is connected

    // --- Create LED and Button as before ---
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