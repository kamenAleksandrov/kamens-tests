idf_component_register(SRCS "WiFi.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi esp_netif LED_Controler WEB_Server)
