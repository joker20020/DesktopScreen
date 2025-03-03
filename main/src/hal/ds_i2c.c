/* i2c - Example

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   See README.md file to get detailed usage of this example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "ds_i2c.h"

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

//设置读取地址
static void i2c_master_set_addr(uint8_t u8Cmd){
    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, (ESP_SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    // i2c_master_write_byte(cmd, u8Cmd, ACK_CHECK_EN);
    // i2c_master_stop(cmd);
    // esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    // i2c_cmd_link_delete(cmd);
    // if (ret != ESP_OK) {
    //     printf("i2c_master_set_addr error !! check tp is connect ?\n");
    // }
    // return ret;
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, &u8Cmd, 1, -1));
}

//读取数据
void i2c_master_read_slave(uint8_t u8Cmd, uint8_t *data_rd, size_t size)
{
    // if (size == 0) {
    //     return ESP_OK;
    // }
    i2c_master_set_addr(u8Cmd);

    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, (ESP_SLAVE_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
    // for(int index=0;index<(size-1);index++)
	// {	   
	// 	i2c_master_read_byte(cmd, data_rd+index, ACK_VAL);
	// }
	// i2c_master_read_byte(cmd, data_rd+size-1, NACK_VAL);

    // i2c_master_stop(cmd);
    // esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    // i2c_cmd_link_delete(cmd);
    // if (ret != ESP_OK) {
    //     printf("i2c_master_read_slave error !! check tp is connect ?\n");
    // }
    // return ret;
    ESP_ERROR_CHECK(i2c_master_receive(dev_handle, data_rd, size, -1));
}

//写入数据
void i2c_master_write_slave(uint8_t u8Cmd, uint8_t *data_wr, size_t size)
{
    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, (ESP_SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    // i2c_master_write_byte(cmd, u8Cmd, ACK_CHECK_EN);
    // i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    // i2c_master_stop(cmd);

    // esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    // i2c_cmd_link_delete(cmd);
    // if (ret != ESP_OK) {
    //     printf("i2c_master_write_slave error\n");
    // }
    // return ret;
    uint8_t data[size + 1] = {};
    memcpy(data, &u8Cmd, 1);
    memcpy(data + 1, data_wr, size);

    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, data, size + 1, -1));
}

/**
 * @brief i2c master initialization
 */
void i2c_master_init(void)
{
    // int i2c_master_port = I2C_MASTER_NUM;
    // i2c_config_t conf;
    // conf.mode = I2C_MODE_MASTER;
    // conf.sda_io_num = I2C_MASTER_SDA_IO;
    // conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    // conf.scl_io_num = I2C_MASTER_SCL_IO;
    // conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    // conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    // i2c_param_config(i2c_master_port, &conf);
    // return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ESP_SLAVE_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

}


