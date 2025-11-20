#include "header.h"
#include "wifi_functions.c"

#define log(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)

void app_main(void)
{
    /* --- CONNECT TO WIFI --- */
    connect();

}
