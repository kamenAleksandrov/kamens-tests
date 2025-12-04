idf_component_register(SRCS "WEB_Server.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_server LED_Controler Storage_Manager)
