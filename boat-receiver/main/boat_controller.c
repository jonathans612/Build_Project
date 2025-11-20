#include "header.h"
#include "wifi_functions.c"

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


}
