idf_component_register(SRCS "main.c"
                            "Hardware/STA/Wifi_STA.c"
                            "Hardware/DHT11/DHT11.c"
                            "Hardware/OLED/oled.c"
                            "MQTT/Mqtt.c"
                    INCLUDE_DIRS "."
                                 "./Hardware/STA"
                                 "./Hardware/DHT11" 
                                 "./Hardware/OLED"
                    
                    )
