idf_component_register(SRCS "boat_controller.c" "wifi_cred.c"
                       PRIV_REQUIRES 
                            spi_flash 
                            nvs_flash
                            esp_netif
                            esp_event
                            esp_wifi
                       INCLUDE_DIRS "")
