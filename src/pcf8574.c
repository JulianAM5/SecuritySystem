#include "pcf8574.h"
#include "i2c.h"

uint8_t pcf8574_write(uint8_t addr, uint8_t value)
{
    uint8_t err = i2c_start((uint8_t)(addr << 1));   /* R/W = 0 */
    if (!err) err = i2c_write(value);
    i2c_stop();
    return err;
}

uint8_t pcf8574_read(uint8_t addr)
{
    uint8_t value = 0xFF;
    if (!i2c_start((uint8_t)((addr << 1) | 1))) {     /* R/W = 1 */
        value = i2c_read(0);                          /* NACK: ultimo byte */
    }
    i2c_stop();
    return value;
}
