#include "header.h"
#include "mqtt_start.h"
#include "motor_driver.h"

#define BROKER_URI "mqtt://test.mosquitto.org:1883"
#define TOPIC "web/initbuild2025/boat/movement"

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    // event->data is provided as char*
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "MQTT connected\n");
            esp_mqtt_client_subscribe(event->client, TOPIC, 0);
            break;

        case MQTT_EVENT_DATA:
            direction_t direction = event->data[0];
            driver(direction);
            break;

        default:
            break;
    }
}

void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URI,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

