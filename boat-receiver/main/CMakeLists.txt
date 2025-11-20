idf_component_register(SRCS "boat_controller.c" "wifi_functions.c"
                       PRIV_REQUIRES 
                            spi_flash 
                            nvs_flash
                            esp_netif
                            esp_event
                            esp_wifi
                            wpa_supplicant
                       INCLUDE_DIRS "")
