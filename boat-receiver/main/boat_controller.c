#include "header.h"
#include "mqtt_start.h"
#include "wifi_functions.c"
#include "motor_driver.h"

#define log(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)

void app_main(void)
{

    /* --- IMPORTANT --- */
    /*
    *  In order to connect to eduroam, you need to supply your credentials.
    *  Make sure you have wifi_login.h in this file's directory with two macros:
    *      #define USER "your_username@fiu.edu"
    *      #define PASS "your_password"
    *  The connect() function will do the rest 
    * 
    */ 
    connect();

    // Start MQTT handling 
    // NOTE: MQTT handler logic will be defined in a separate file and invoked in the mqtt_functions.c code
    mqtt_start();

    esc_init();
}
