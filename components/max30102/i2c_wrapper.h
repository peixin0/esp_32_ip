#ifndef I2C_WRAPPER_H_
#define I2C_WRAPPER_H_



#include "driver/i2c.h"
#include "esp_err.h"



#define I2C_MASTER_SCL_IO               22                  /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO               21                  /*!< gpio number for I2C master data  */
#define I2C_FREQ_HZ                     100000              /*!< I2C master clock frequency */
#define I2C_PORT_NUM                    I2C_NUM_0           /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE       0                   /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE       0                   /*!< I2C master doesn't need buffer */


esp_err_t i2c_write_max(uint8_t addr, uint8_t *data,uint8_t len, bool ack_en);
esp_err_t i2c_read_max (uint8_t addr, uint8_t *data,uint8_t len, bool ack_en);


#endif /* I2C_WRAPPER_H_ */