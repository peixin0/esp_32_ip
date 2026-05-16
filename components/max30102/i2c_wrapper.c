#include "i2c_wrapper.h"

esp_err_t i2c_master_init(void){
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master.clk_speed  = I2C_FREQ_HZ
        };
    esp_err_t ret = i2c_param_config(I2C_PORT_NUM,&conf);       // could error handler 
    if (ret != ESP_OK) {
        return ret;
    }
    ret = i2c_driver_install(I2C_PORT_NUM,conf.mode,I2C_MASTER_RX_BUF_DISABLE,I2C_MASTER_TX_BUF_DISABLE,0); // could error handler 
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;

}

esp_err_t i2c_write_max(uint8_t addr, const uint8_t *data, uint8_t len, bool repeated) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr, true);      // send device address 0xAE
    i2c_master_write(cmd, data, len, true);      // send data, len bytes, already包含寄存器地址+数据
    if (!repeated) i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t i2c_read_max(uint8_t addr, const uint8_t *data, uint8_t len, bool repeated) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr, true);      // send device address 0xAF
    if (len > 1) i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    if (!repeated) i2c_master_stop(cmd);            
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(100));       
    i2c_cmd_link_delete(cmd);
    return ret;
}