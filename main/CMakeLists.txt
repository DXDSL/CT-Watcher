idf_component_register(
    SRCS 
        "main.c"
        "Hardware/DHT11/DHT11.c"
        "NETWORK/MQTT/mqtt.c"
        "Hardware/HumanIR/bsp_HumanIR.c"
        "AUDIO/bsp_Audio.c"
        "NETWORK/PROV/ble_prov.c"
        "NETWORK/WebConfig/web_config.c"
        "Hardware/OLED/bsp_oled.c"

    INCLUDE_DIRS 
        "."
        "Hardware/DHT11"
        "Hardware/HumanIR"
        "AUDIO"
        "NETWORK/MQTT"
        "NETWORK/PROV"
        "NETWORK/WebConfig"
        "Hardware/OLED"

    EMBED_TXTFILES
        "NETWORK/WebConfig/index.html"

    REQUIRES 
        esp_audio_codec
        spiffs
        driver
        esp_timer
        mqtt
        nvs_flash
        esp_event
        esp_wifi
        wifi_provisioning
        json
        esp_http_server
)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)