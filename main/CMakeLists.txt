idf_component_register(
    SRCS 
        "main.c"
        "NETWORK/STA/Wifi_STA.c"
        "Hardware/DHT11/DHT11.c"
        "Hardware/OLED/oled.c"
        "NETWORK/MQTT/Mqtt.c"
        "Hardware/HumanIR/bsp_HumanIR.c"
        "AUDIO/bsp_Audio.c"

    INCLUDE_DIRS 
        "."
        "NETWORK/STA"
        "Hardware/DHT11"
        "Hardware/OLED"
        "Hardware/HumanIR"
        "AUDIO"
        "NETWORK/MQTT"

    REQUIRES 
        esp_audio_codec
        spiffs
        driver
        esp_timer
        mqtt
        nvs_flash
        esp_event
        esp_wifi
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)