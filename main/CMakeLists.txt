
idf_component_register(
                        SRCS "main.c"

                                "src/net/ds_wifi_scan.c"
                                "src/net/ds_wifi_ap.c"
                                "src/net/ds_https_request.c"
                                "src/net/ds_http_server.c"
                                "src/net/ds_wifi_sta.c"
                                "src/net/ds_http_request.c"
                                "src/net/ds_wifi_ap_sta.c"
                                "src/net/ds_dns_server.c"
                                "src/net/ds_sntp.c"

                                "./src/driver/ds_ft6336.c"
                                "./src/driver/ds_screen.c"

                                "src/hal/ds_timer.c"
                                "src/hal/ds_gpio.c"
                                "src/hal/ds_i2c.c"
                                "src/hal/ds_nvs.c"
                                "src/hal/ds_spi.c"
                                "src/hal/ds_pwm.c"

                                "src/fs/ds_spiffs.c"

                                "src/ds_system_data.c"

                                "src/data/ds_data_weather.c"
                                "src/data/ds_data_num.c"
                                "src/data/ds_data_icon.c"
                                "src/data/ds_data_page.c"

                                "src/draw/ds_font.c"
                                "src/draw/ds_paint.c"

                        INCLUDE_DIRS 
                                "."
                                "./include"
                                "./include/draw"
                                "./include/data"
                                "./include/hal"
                                "./include/net"
                                "./include/driver"
                                "./include/fs"
                                "./include/ui"
                               

                        EMBED_FILES
                                
                        EMBED_TXTFILES
                                
                        )
