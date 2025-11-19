#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "tags.h"

#define log(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)

#include "wifi_cred.c"

void app_main(void)
{
    /* --- CONNECT TO WIFI --- */
    log(WIFI, "CONNECTING TO WIFI...");
    
    esp_err_t status;

    status = nvs_flash_init();
    ESP_ERROR_CHECK(status);

    status = esp_netif_init();
    ESP_ERROR_CHECK(status);

    status = esp_event_loop_create_default();
    ESP_ERROR_CHECK(status);

    log(TEST, teststr);

}
