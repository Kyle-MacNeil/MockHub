#pragma once
#include <stdint.h>
#include "common.h"
#include "I2CCommon.h"

void open_I2C_bus(void);

void close_I2C_bus(void);

void set_I2C_device(uint8_t address);

void write_I2C(uint8_t address, uint8_t reg, uint8_t data);

uint8_t read_I2C(uint8_t address, uint8_t reg);

void write_I2C_with_mask(uint8_t address, uint8_t reg, uint8_t data, uint8_t mask);