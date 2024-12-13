idf_component_register(
    SRCS "main_esp.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        bt 
        nvs_flash 
        driver 
        esp_common 
        esp_event
)