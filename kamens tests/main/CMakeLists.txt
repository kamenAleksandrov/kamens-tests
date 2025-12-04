idf_component_register(
    SRCS "station_example_main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash BLE WiFi LED_Controler Storage_Manager
)