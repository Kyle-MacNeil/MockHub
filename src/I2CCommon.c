
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include "smbus.h"

#include "common.h"
#include "I2CCommon.h"
#include "util.h"

#define I2C_BUS "/dev/i2c-2"

static int FD = -1;
static uint8_t AddressOfCurrentI2CDevice = 0;

void open_I2C_bus(void)
{
	//LOG_MSG("Open I2C_bus\n");
	if ((FD = open(I2C_BUS, O_RDWR)) < 0)
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to open I2C bus.");
		//log_error(__func__, "Failed to open I2C bus");
	}
	
}

void close_I2C_bus(void)
{
	//LOG_MSG("Close I2C_bus\n");
	close(FD);
}

void set_I2C_device(uint8_t address)
{
	//LOG_MSG("Set I2C_device\n");
	if (ioctl(FD, I2C_SLAVE, address) < 0) {
		close(FD);
		LOG_ERROR(CRITICAL_ERROR, "Failed to open address of the I2C slave device.");
		//log_error(__func__, "Failed to open address of the I2C slave device");
	}
}

void write_I2C(uint8_t address, uint8_t reg, uint8_t data)
{
	//LOG_MSG("Write I2C\n");
	if (address != AddressOfCurrentI2CDevice)
	{
		//LOG_MSG("Different address, setting I2C device\n");
		set_I2C_device(address);
	}
	
	if (i2c_smbus_write_byte_data(FD, reg, data) < 0) {
		// error
		close(FD);
		LOG_ERROR(CRITICAL_ERROR, "Failed to write to I2C bus.");
		//log_error(__func__, "Failed to write to I2C bus");
	}
	else {
		//LOG_MSG("Data 0x%02X written to register 0x%02X\n", data, reg);
		printf("Data 0x%02X written to register 0x%02X\n", data, reg);
	}
}

uint8_t read_I2C(uint8_t address, uint8_t reg)
{
	//LOG_MSG("Read I2C\n");
	uint8_t data = 0;
	
	if (address != AddressOfCurrentI2CDevice)
	{
		//LOG_MSG("Different address, setting I2C device\n");
		set_I2C_device(address);
	}
	
	data = i2c_smbus_read_byte_data(FD, reg);
	if (data < 0)
	{
		close(FD);
		LOG_ERROR(CRITICAL_ERROR, "Failed to read from I2C bus.");
		//log_error(__func__, "Failed to read from I2C bus");
	}
	else
	{
		//LOG_MSG("Data read from register 0x%02X: 0x%02X\n", reg, data);
		printf("Data read from register 0x%02X: 0x%02X\n", reg, data);
	}
	return data;
}

void write_I2C_with_mask(uint8_t address, uint8_t reg, uint8_t data, uint8_t mask)
{
	//LOG_MSG("Write I2C with mask\n");
	uint8_t currentdata = 0;
	
	if (address != AddressOfCurrentI2CDevice)
	{
		//LOG_MSG("Different address, setting I2C device\n");
		set_I2C_device(address);
	}
	
	// Read the current value of the register
	currentdata = i2c_smbus_read_byte_data(FD, reg);
	if (data < 0)
	{
		close(FD);
		LOG_ERROR(CRITICAL_ERROR, "Failed to read from I2C bus.");
		//log_error(__func__, "Failed to read from I2C bus");
	}
	else
	{
		//LOG_MSG("Data read from register 0x%02X: 0x%02X\n", reg, data);
		printf("Data read from register 0x%02X: 0x%02X\n", reg, data);
	}
	
	// Clear the bits that are masked
	currentdata &= ~mask;
	// Set the bits that are masked with the new value
	currentdata |= (data & mask);
	
	// Write the updated value to the register
	if (i2c_smbus_write_byte_data(FD, reg, currentdata) < 0) {
		// error
		close(FD);
		LOG_ERROR(CRITICAL_ERROR, "Failed to write to I2C bus.");
		//log_error(__func__, "Failed to write to I2C bus");
	}
	else {
		//LOG_MSG("Data 0x%02X written to register 0x%02X\n", data, reg);
		printf("Data 0x%02X written to register 0x%02X\n", data, reg);
	}
}