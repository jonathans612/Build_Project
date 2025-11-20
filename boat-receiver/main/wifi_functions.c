#include "header.h"
#include "wifi_login.h"

void connect(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "eduroam",
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_eap_client_set_identity((uint8_t *)USER, strlen(USER));
    esp_eap_client_set_username((uint8_t *)USER, strlen(USER));
    esp_eap_client_set_password((uint8_t *)PASS, strlen(PASS));

    esp_eap_client_set_eap_methods(ESP_EAP_TYPE_PEAP);
    esp_eap_client_use_default_cert_bundle(true);

    ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}