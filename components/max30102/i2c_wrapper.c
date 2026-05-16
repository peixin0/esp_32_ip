#include "i2c_wrapper.h"

esp_err_t i2c_master_init(void){
    i2c_config_t conf = {
        .master = I2C_MODE_MASTER,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = true,
        .sdl_pullup_en = true,
        .master.clk_speed  = I2C_FREQ_HZ
        }
    i2c_param_config(I2C_PORT_NUM,&conf);       // could error handler 
    i2c_driver_install(I2C_PORT_NUM,conf.master,I2C_MASTER_RX_BUF_DISABLE,I2C_MASTER_TX_BUF_DISABLE,0); // could error handler 
    return ESP_OK;

}

esp_err_t i2c_write_max(uint8_t addr, uint8_t *data,uint8_t len, bool ack_en){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, data, len, ack_en);
    i2c_master_stop(cmd);
    i2c_cmd_link_delete(cmd);
    return ESP_OK;
}


esp_err_t i2c_read_max (uint8_t addr, uint8_t *data,uint8_t len, bool ack_en)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, data, len, ack_en);
    i2c_master_stop(cmd);
    i2c_cmd_link_delete(cmd);
    return ESP_OK;
}