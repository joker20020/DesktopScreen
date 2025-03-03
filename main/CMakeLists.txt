
idf_component_register(
                        SRCS "main.c"

                                "./src/driver/ds_ft6336.c"
                                "./src/driver/ds_screen.c"

                                "src/hal/ds_timer.c"
                                "src/hal/ds_gpio.c"
                                "src/hal/ds_i2c.c"
                                "src/hal/ds_nvs.c"
                                "src/hal/ds_spi.c"
                                "src/hal/ds_pwm.c"

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
