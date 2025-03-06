
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "ds_screen.h"
#include "ds_ui_mainpage.h"
#include "ds_system_data.h"
#include "ds_spi.h"

#include "ds_data_page.h"
#include "ds_wifi_ap_sta.h"

#include "ds_ui_qrpage.h"


void ds_ui_qrcode_show(){
	ds_screen_full_display_bydata(ds_screen_full_display_data,gImage_qrcode_page);
}  

