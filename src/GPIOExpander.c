

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <err.h>
#include <errno.h>

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "common.h"
#include "I2CCommon.h"
#include "smbus.h"
#include "ChannelStatus.h"
#include "GPIOExpander.h"
#include "interrupts.h"
#include "LED.h"
#include "LoopSetup.h"
#include "util.h"

typedef enum
{
	RX_LOS        = 0,
	TX_Fault,
	TX_Disconnect,
	Module_Absent
}IOExpanderEnum;

typedef enum
{
	previous,
	current,
	max
}ExpanderStateIndex;

typedef enum
{
	Read__Input_Port_0         = 0,
	Read__Input_Port_1         = 1,
	RW__Output_Port_0          = 2,
	RW__Output_Port_1          = 3,
	RW__PolarityInverse_Port_0 = 4,
	RW__PolarityInverse_Port_1 = 5,
	RW__Configuration_Port_0   = 6,
	RW__Configuration_Port_1   = 7,
}ExpandrCommands;

typedef struct
{
	union
	{
		uint16_t data;
		uint8_t Port0;
		uint8_t Port1;		
	};    
}IOData;

typedef struct 
{
	IOData InputData;
	IOData OutputData;    
	IOData PolarityData;
	IOData ConfigData;
}IOExpanderData;


/*
 * Notes:
 * 
 * This is the gpioexpander, ther are 4 of them.
 * 
 * Addresses:
 * 0x20, 0x21, 0x22 and 0x23
 *
 **/
#define IOExpanderFor_RXLossOfSignalx I2CMuxDevice20
#define IOExpanderFor_TXFaultx        I2CMuxDevice21
#define IOExpanderFor_TXDisconnectx   I2CMuxDevice22
#define IOExpanderFor_MODAbsentx      I2CMuxDevice23

static ExpanderStates ExpanderStatesArray[max];

static IOExpanderData IOExpanderDataArray[4];

void get_GPIO_expander_state();

static int GPIOExpanderHandle = -1;
static bool InitializeIOExpandersComplete = false;

static bool set_configuration_GPIO_device(int GPIOExpanderHandle, I2CMuxDevice Address, uint16_t* config);
static void read_device(int GPIOExpanderHandle, I2CMuxDevice Address);


ExpanderStates* init_get_GPIO_expander_state(void)
{
	LOG_MSG("Get GPIO expander state\n");
	get_GPIO_expander_state(); 
	
	set_hub_mode(ExpanderStatesArray[current].rx_loss.S_D_Sel);
	
	return &ExpanderStatesArray[current];
}


void configure_GPIO_expander( void )
{
	LOG_MSG("Configure GPIO expander\n");
    // configure each device
	BitsRXLOSS bitsRXLOSS;
	BitsTXFault bitsTXFault;
	TXDisableBits TXdisableBits;
	ModuleAbsentBits moduleAbsentBits;
	
    memset(&bitsRXLOSS, 0, sizeof(BitsRXLOSS));
	memset(&bitsTXFault, 0, sizeof(BitsTXFault));
	memset(&TXdisableBits, 0, sizeof(BitsTXFault));
	memset(&moduleAbsentBits, 0, sizeof(BitsTXFault));
	
	//KM: bitsRXLOSS
    bitsRXLOSS.RX_LOS0 = SetAsInputPin;
    bitsRXLOSS.RX_LOS1 = SetAsInputPin;
    bitsRXLOSS.RX_LOS2 = SetAsInputPin;
    bitsRXLOSS.RX_LOS3 = SetAsInputPin;
    bitsRXLOSS.RX_LOS4 = SetAsInputPin;
    bitsRXLOSS.RX_LOS5 = SetAsInputPin;
    bitsRXLOSS.RX_LOS6 = SetAsInputPin;
    bitsRXLOSS.RX_LOS7 = SetAsInputPin;
    bitsRXLOSS.RX_LOS8 = SetAsInputPin;
    bitsRXLOSS.RX_LOS9 = SetAsInputPin;
    bitsRXLOSS.Soft_Reset_l = SetAsInputPin;
    bitsRXLOSS.S_D_Sel = SetAsInputPin;
        
	//KM: bitsTXFault
    bitsTXFault.TX_Fault_0 = SetAsInputPin;
    bitsTXFault.TX_Fault_1 = SetAsInputPin;
    bitsTXFault.TX_Fault_2 = SetAsInputPin;
    bitsTXFault.TX_Fault_3 = SetAsInputPin;
    bitsTXFault.TX_Fault_4 = SetAsInputPin;
    bitsTXFault.TX_Fault_5 = SetAsInputPin;
    bitsTXFault.TX_Fault_6 = SetAsInputPin;
    bitsTXFault.TX_Fault_7 = SetAsInputPin;
    bitsTXFault.TX_Fault_8 = SetAsInputPin;
    bitsTXFault.TX_Fault_9 = SetAsInputPin;
	
	//KM: TXdisableBits
    TXdisableBits.TX_Disable0 = SetAsOutputPin;
    TXdisableBits.TX_Disable1 = SetAsOutputPin;
    TXdisableBits.TX_Disable2 = SetAsOutputPin;
    TXdisableBits.TX_Disable3 = SetAsOutputPin;
    TXdisableBits.TX_Disable4 = SetAsOutputPin;
    TXdisableBits.TX_Disable5 = SetAsOutputPin;
    TXdisableBits.TX_Disable6 = SetAsOutputPin;
    TXdisableBits.TX_Disable7 = SetAsOutputPin;
    TXdisableBits.TX_Disable8 = SetAsOutputPin;
    TXdisableBits.TX_Disable9 = SetAsOutputPin;
	TXdisableBits.EEProm_wr_l = SetAsOutputPin;
    TXdisableBits.MUX_RST = SetAsOutputPin;
    TXdisableBits.TR_RS0 = SetAsOutputPin;
    TXdisableBits.TR_RS1 = SetAsOutputPin;
        
	//KM: moduleAbsentBits
    moduleAbsentBits.MOD_ABS0 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS1 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS2 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS3 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS4 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS5 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS6 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS7 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS8 = SetAsInputPin;
    moduleAbsentBits.MOD_ABS9 = SetAsInputPin;

// write to all GPIO Expanders     
	set_configuration_GPIO_device(GPIOExpanderHandle, IOExpanderFor_RXLossOfSignalx, (uint16_t*) &bitsRXLOSS);
	set_configuration_GPIO_device(GPIOExpanderHandle, IOExpanderFor_TXFaultx, (uint16_t*) &bitsTXFault);
	set_configuration_GPIO_device(GPIOExpanderHandle, IOExpanderFor_TXDisconnectx, (uint16_t*) &TXdisableBits);
	set_configuration_GPIO_device(GPIOExpanderHandle, IOExpanderFor_MODAbsentx, (uint16_t*) &moduleAbsentBits);

    printf("configure_GPIO_expander ends \n");
    LOG_MSG("configure_GPIO_expander ends \n");
}

static bool set_configuration_GPIO_device(int Handle, I2CMuxDevice Address, uint16_t* config)
{ 
	LOG_MSG("Set GPIO device configuration \n");
	uint8_t value = 0;
	uint8_t command = 20; // 20 is an invalid command (this is to inovke an error if a command is not assigned somehow)
	
    switch (Address)
    {
    case IOExpanderFor_RXLossOfSignalx:
        {
	        //Ensure that the GPIO Expanders at 0x20 are set to the correct Polarity
	        LOG_MSG("Setting configuration for RXLossOfSignalx\n");
	        command = RW__PolarityInverse_Port_0;  //Port0
	        memset(&value, 0, 1);
	        write_I2C(IOExpanderFor_RXLossOfSignalx, command, value);
	        
	        command = RW__PolarityInverse_Port_1; //Port1 
	        write_I2C(IOExpanderFor_RXLossOfSignalx, command, value);
	        
	        //Now Configure GPIO Expander at 0x20
	        command = RW__Configuration_Port_0;  //Port0
	        memcpy(&value, config, 1);
	        write_I2C(IOExpanderFor_RXLossOfSignalx, command, value);
	        
	        command = RW__Configuration_Port_1;  //Port1
	        value = (uint8_t)((*config & 0xFF00) >> 8);
	        write_I2C(IOExpanderFor_RXLossOfSignalx, command, value);
                  
        }break;
        
    case IOExpanderFor_TXFaultx:
        {
			LOG_MSG("Setting configuration for TXFaultx\n");
	        //Ensure that the GPIO Expanders at 0x21 are set to the correct Polarity
	        command = RW__PolarityInverse_Port_0; //Port0
	        memset(&value, 0, 1);
	        write_I2C(IOExpanderFor_TXFaultx, command, value);
	        
	        command = RW__PolarityInverse_Port_1; //Port1 
	        write_I2C(IOExpanderFor_TXFaultx, command, value);
           
	        //Now Configure GPIO Expander at 0x21
	        command = RW__Configuration_Port_0; //Port0
	        memcpy(&value, config, 1);
	        write_I2C(IOExpanderFor_TXFaultx, command, value);
	        
	        command = RW__Configuration_Port_1; //Port1
	        value = (*config & 0xFF00) >> 8;
	        write_I2C(IOExpanderFor_TXFaultx, command, value);

        }break;

    case IOExpanderFor_TXDisconnectx:
        {  
			LOG_MSG("Setting configuration for TXDisconnectx\n");
	        //Ensure that the GPIO Expanders at 0x22 are set to the correct Polarity
	        command = RW__PolarityInverse_Port_0; //Port0
	        memset(&value, 0, 1);
	        write_I2C(IOExpanderFor_TXDisconnectx, command, value);
	        
	        command = RW__PolarityInverse_Port_1; //Port1 
	        write_I2C(IOExpanderFor_TXDisconnectx, command, value);
           
	        //Now Configure GPIO Expander at 0x22
	        command = RW__Configuration_Port_0; // Port0
	        memcpy(&value, config, 1);
	        write_I2C(IOExpanderFor_TXDisconnectx, command, value);
	        
	        command = RW__Configuration_Port_1; //Port1
	        value = (*config & 0xFF00) >> 8;
	        write_I2C(IOExpanderFor_TXDisconnectx, command, value);
	        
	        //Now set output signals to low
	        command = RW__Output_Port_0; //Port0
	        value = 0x00;
	        write_I2C(IOExpanderFor_TXDisconnectx, command, value);
	        
	        command = RW__Output_Port_1; //Port1
	        value = 0x00;
	        write_I2C(IOExpanderFor_TXDisconnectx, command, value);
	        
        }break;

    case IOExpanderFor_MODAbsentx:
        {
	        LOG_MSG("Setting configuration for MODAbsentx\n");
	        //Ensure that the GPIO Expanders at 0x23 are set to the correct Polarity
	        command = RW__PolarityInverse_Port_0; //Port0
	        memset(&value, 0, 1);
	        write_I2C(IOExpanderFor_MODAbsentx, command, value);
	        
	        //Port1 
	        command = RW__PolarityInverse_Port_1;
	        write_I2C(IOExpanderFor_MODAbsentx, command, value);
           
	        //Now Configure GPIO Expander at 0x23
	        command = RW__Configuration_Port_0; //Port0
	        memcpy(&value, config, 1);
	        write_I2C(IOExpanderFor_MODAbsentx, command, value);
	        
	        command = RW__Configuration_Port_1; //Port1
	        value = (*config & 0xFF00) >> 8;
	        write_I2C(IOExpanderFor_MODAbsentx, command, value);
            
        }break;

    default:
        {
			LOG_ERROR(CRITICAL_ERROR, "Error, entered the default switch case (not suppose to happen)." );
	        //log_error(__func__, "Error, entered the default switch case (not suppose to happen)");       
        }break;
    }    
	
	
}

void clear_IO_expanders_interrupt(void)
{   
	LOG_MSG("clear IO expanders interrupt\n");   
    // read all fields        
    read_device(GPIOExpanderHandle, IOExpanderFor_RXLossOfSignalx);
    read_device(GPIOExpanderHandle, IOExpanderFor_TXFaultx);
    read_device(GPIOExpanderHandle, IOExpanderFor_TXDisconnectx);
    read_device(GPIOExpanderHandle, IOExpanderFor_MODAbsentx);
}


static void read_device(int Handle, I2CMuxDevice Address)
{ 
    switch (Address)
    {
    case IOExpanderFor_RXLossOfSignalx:
        {
			LOG_MSG("Reading device RXLossOfSignalx\n");
            uint8_t  command = Read__Input_Port_0;
	        IOExpanderDataArray[RX_LOS].InputData.Port0 = read_I2C(IOExpanderFor_RXLossOfSignalx, command);
	        
            command = Read__Input_Port_1;
	        IOExpanderDataArray[RX_LOS].InputData.Port1 = read_I2C(IOExpanderFor_RXLossOfSignalx, command);
	        
        }break;
        
    case IOExpanderFor_TXFaultx:
        {
			LOG_MSG("Reading device TXFaultx\n");
            uint8_t  command = Read__Input_Port_0;
	        IOExpanderDataArray[TX_Fault].InputData.Port0 = read_I2C(IOExpanderFor_TXFaultx, command);
	        
            command = Read__Input_Port_1;
	        IOExpanderDataArray[TX_Fault].InputData.Port1 = read_I2C(IOExpanderFor_TXFaultx, command);
	        
        }break;

    case IOExpanderFor_TXDisconnectx:
        { 
			LOG_MSG("Reading device TXDisconnectx\n");
            uint8_t  command = Read__Input_Port_0;
	        IOExpanderDataArray[TX_Disconnect].InputData.Port0 = read_I2C(IOExpanderFor_TXDisconnectx, command);
	        
            command = Read__Input_Port_1;
	        IOExpanderDataArray[TX_Disconnect].InputData.Port1 = read_I2C(IOExpanderFor_TXDisconnectx, command);
            
        }break;

    case IOExpanderFor_MODAbsentx:
        {
			LOG_MSG("Reading device MODAbsentx\n");
            uint8_t  command = Read__Input_Port_0;
	        IOExpanderDataArray[Module_Absent].InputData.Port0 = read_I2C(IOExpanderFor_MODAbsentx, command);
	   
            command = Read__Input_Port_1;
	        IOExpanderDataArray[Module_Absent].InputData.Port1 = read_I2C(IOExpanderFor_MODAbsentx, command);
            
        }break;

    default:
        {
			LOG_ERROR(CRITICAL_ERROR, "Error, entered the default switch case (not suppose to happen).");
	        //log_error(__func__, "Error, entered the default switch case (not suppose to happen)");           
        }break;
    }    
}


void get_GPIO_expander_state(void)
{ 
	LOG_MSG("Get GPIO expander state\n");
    uint8_t command = 0;
    uint8_t port0 = 0;
    uint8_t port1 = 0;
    
    // Filling rx_loss
    command = Read__Input_Port_0;
	port0 = read_I2C(IOExpanderFor_RXLossOfSignalx, command);
	
	
	
	ExpanderStatesArray[current].rx_loss.RX_LOS0 = (port0 >> 0) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS1 = (port0 >> 1) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS2 = (port0 >> 2) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS3 = (port0 >> 3) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS4 = (port0 >> 4) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS5 = (port0 >> 5) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS6 = (port0 >> 6) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS7 = (port0 >> 7) & 0x01;

    command = Read__Input_Port_1;
	port1 = read_I2C(IOExpanderFor_RXLossOfSignalx, command);  
	
    ExpanderStatesArray[current].rx_loss.RX_LOS8 = (port1 >> 0) & 0x01;
    ExpanderStatesArray[current].rx_loss.RX_LOS9 = (port1 >> 1) & 0x01;
    ExpanderStatesArray[current].rx_loss.reserved_0 = (port1 >> 2) & 0x01;
    ExpanderStatesArray[current].rx_loss.reserved_1 = (port1 >> 3) & 0x01;
    ExpanderStatesArray[current].rx_loss.reserved_2 = (port1 >> 4) & 0x01;
    ExpanderStatesArray[current].rx_loss.reserved_3 = (port1 >> 5) & 0x01;
    ExpanderStatesArray[current].rx_loss.Soft_Reset_l = (port1 >> 6) & 0x01;
    ExpanderStatesArray[current].rx_loss.S_D_Sel      = (port1 >> 7) & 0x01;


    // Filling tx_fault	
    command = Read__Input_Port_0;
	port0 = read_I2C(IOExpanderFor_TXFaultx, command);
	
    ExpanderStatesArray[current].tx_fault.TX_Fault_0 = (port0 >> 0) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_1 = (port0 >> 1) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_2 = (port0 >> 2) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_3 = (port0 >> 3) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_4 = (port0 >> 4) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_5 = (port0 >> 5) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_6 = (port0 >> 6) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_7 = (port0 >> 7) & 0x01;

    command = Read__Input_Port_1;
	port1 = read_I2C(IOExpanderFor_TXFaultx, command);
	
    ExpanderStatesArray[current].tx_fault.TX_Fault_8 = (port1 >> 0) & 0x01;
    ExpanderStatesArray[current].tx_fault.TX_Fault_9 = (port1 >> 1) & 0x01;
    ExpanderStatesArray[current].tx_fault.reserved_0 = (port1 >> 2) & 0x01;
    ExpanderStatesArray[current].tx_fault.reserved_1 = (port1 >> 3) & 0x01;
    ExpanderStatesArray[current].tx_fault.reserved_2 = (port1 >> 4) & 0x01;
    ExpanderStatesArray[current].tx_fault.reserved_3 = (port1 >> 5) & 0x01;
    ExpanderStatesArray[current].tx_fault.reserved_4 = (port1 >> 6) & 0x01;
    ExpanderStatesArray[current].tx_fault.reserved_5 = (port1 >> 7) & 0x01;


    // Filling tx_disable
    command = Read__Input_Port_0;
	port0 = read_I2C(IOExpanderFor_TXDisconnectx, command);
	
    ExpanderStatesArray[current].tx_disable.TX_Disable0 = (port0 >> 0) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable1 = (port0 >> 1) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable2 = (port0 >> 2) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable3 = (port0 >> 3) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable4 = (port0 >> 4) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable5 = (port0 >> 5) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable6 = (port0 >> 6) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable7 = (port0 >> 7) & 0x01;

    command = Read__Input_Port_1;
	port1 = read_I2C(IOExpanderFor_TXDisconnectx, command);
	
    ExpanderStatesArray[current].tx_disable.TX_Disable8 = (port1 >> 0) & 0x01;
    ExpanderStatesArray[current].tx_disable.TX_Disable9 = (port1 >> 1) & 0x01;
    ExpanderStatesArray[current].tx_disable.reserved_0 = (port1 >> 2) & 0x01;
    ExpanderStatesArray[current].tx_disable.reserved_1 = (port1 >> 3) & 0x01;
    ExpanderStatesArray[current].tx_disable.EEProm_wr_l = (port1 >> 4) & 0x01;
    ExpanderStatesArray[current].tx_disable.MUX_RST = (port1 >> 5) & 0x01;
    ExpanderStatesArray[current].tx_disable.TR_RS0 = (port1 >> 6) & 0x01;
    ExpanderStatesArray[current].tx_disable.TR_RS1 = (port1 >> 7) & 0x01;


    // Filling mod_absent
    command = Read__Input_Port_0;
	port0 = read_I2C(IOExpanderFor_MODAbsentx, command);
	
    ExpanderStatesArray[current].mod_absent.MOD_ABS0 = (port0 >> 0) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS1 = (port0 >> 1) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS2 = (port0 >> 2) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS3 = (port0 >> 3) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS4 = (port0 >> 4) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS5 = (port0 >> 5) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS6 = (port0 >> 6) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS7 = (port0 >> 7) & 0x01;

    command = Read__Input_Port_1;
	port1 = read_I2C(IOExpanderFor_MODAbsentx, command);
	
    ExpanderStatesArray[current].mod_absent.MOD_ABS8 = (port1 >> 0) & 0x01;
    ExpanderStatesArray[current].mod_absent.MOD_ABS9 = (port1 >> 1) & 0x01;
    ExpanderStatesArray[current].mod_absent.reserved_0 = (port1 >> 2) & 0x01;
    ExpanderStatesArray[current].mod_absent.reserved_1 = (port1 >> 3) & 0x01;
    ExpanderStatesArray[current].mod_absent.reserved_2 = (port1 >> 4) & 0x01;
    ExpanderStatesArray[current].mod_absent.reserved_3 = (port1 >> 5) & 0x01;
    ExpanderStatesArray[current].mod_absent.reserved_4 = (port1 >> 6) & 0x01;
    ExpanderStatesArray[current].mod_absent.reserved_5 = (port1 >> 7) & 0x01;


}

ExpanderStates* save_state_GPIO_expander(void)
{
	LOG_MSG("Saving GPIO expander state\n");
	ExpanderStatesArray[previous] = ExpanderStatesArray[current];
	get_GPIO_expander_state();
	
	return &ExpanderStatesArray[current];
}

uint16_t get_SFP_SD(void)
{
	LOG_MSG("Get SFP Signal detect\n");
	BitsRXLOSS Inverse_SD;
	uint16_t SD_Converted =0;
	uint8_t command = 0;
	uint8_t port0 = 0;
	uint8_t port1 = 0;
	
	// Filling Inverse_SD
	command = Read__Input_Port_0;
	port0 = read_I2C(IOExpanderFor_RXLossOfSignalx, command);
	
	Inverse_SD.RX_LOS0 = (port0 >> 0) & 0x01;
	Inverse_SD.RX_LOS1 = (port0 >> 1) & 0x01;
	Inverse_SD.RX_LOS2 = (port0 >> 2) & 0x01;
	Inverse_SD.RX_LOS3 = (port0 >> 3) & 0x01;
	Inverse_SD.RX_LOS4 = (port0 >> 4) & 0x01;
	Inverse_SD.RX_LOS5 = (port0 >> 5) & 0x01;
	Inverse_SD.RX_LOS6 = (port0 >> 6) & 0x01;
	Inverse_SD.RX_LOS7 = (port0 >> 7) & 0x01;

	command = Read__Input_Port_1;
	port1 = read_I2C(IOExpanderFor_RXLossOfSignalx, command);
		
	Inverse_SD.RX_LOS8 = (port1 >> 0) & 0x01;
	Inverse_SD.RX_LOS9 = (port1 >> 1) & 0x01;
	Inverse_SD.reserved_0 = (port1 >> 2) & 0x01;
	Inverse_SD.reserved_1 = (port1 >> 3) & 0x01;
	Inverse_SD.reserved_2 = (port1 >> 4) & 0x01;
	Inverse_SD.reserved_3 = (port1 >> 5) & 0x01;
	Inverse_SD.Soft_Reset_l = (port1 >> 6) & 0x01;
	Inverse_SD.S_D_Sel      = (port1 >> 7) & 0x01;
	
	SD_Converted = *((uint16_t*) &Inverse_SD);
	
	SD_Converted ^= 0x03FF;
	
	return SD_Converted;
}

void init_GPIO_expanders(void)
{	
	LOG_MSG("Initialize GPIO expanders\n");
	configure_GPIO_expander();

	LOG_MSG("IOExpanders Configured \r\n");
	printf("IOExpanders Configured \r\n");
	
	clear_IO_expanders_interrupt();
	
}

uint64_t compare_GPIO_states(void)
{
	LOG_MSG("Compare GPIO states, previous and current\n");
	uint64_t *current_ptr = (uint64_t *)&ExpanderStatesArray[current];
	uint64_t *previous_ptr = (uint64_t *)&ExpanderStatesArray[previous];
	uint64_t changedBits = 0;
		
	changedBits = *current_ptr ^ *previous_ptr; // XOR to find changed bits
	
	return changedBits;
}

void assert_TX_disable(uint16_t TXdisable)
{
	LOG_MSG("Assert TX disable\n");
	uint8_t valuePort0 = 0;
	uint8_t valuePort1 = 0;
	int i = 0;
	
	
	//KM: set port 0 of GPIO expander first
	//valuePort0 = read_I2C(IOExpanderFor_TXDisconnectx, RW__Output_Port_0);
	
	for (i = 0; i < 8; i++)
	{
		if ((TXdisable & (1 << i)) > 0)//KM: if TX is disabled on channel i
		{
			valuePort0 |= (1 << i);//KM: Disable channel i
		}
		else
		{
			valuePort0 &= ~(1 << i);//KM: Do not disable channel i
		}
		
	}
	
	write_I2C(IOExpanderFor_TXDisconnectx, RW__Output_Port_0, valuePort0);
	
	//KM: set port 1 of GPIO expander 
	valuePort1 = read_I2C(IOExpanderFor_TXDisconnectx, RW__Output_Port_1);
	
	for (i = 8; i < 10; i++)
	{
		if ((TXdisable & (1 << i)) > 0)//KM: if TX is disabled on channel i
		{
			valuePort1 |= (1 << (i-8)); //KM: Disable TX on channel i
		}
		else
		{
			valuePort1 &= ~(1 << (i - 8)); //KM: Do not disable TX on channel i
		}
		
	}
	
	write_I2C(IOExpanderFor_TXDisconnectx, RW__Output_Port_1, valuePort1);
	
}

void change_hub_mode(void)
{
	LOG_MSG("Changing hub mode\n");
	set_hub_mode(ExpanderStatesArray[current].rx_loss.S_D_Sel);
}

uint16_t get_reset_pin_state(void)
{
	LOG_MSG("Get reset pin state\n");
	uint8_t command = 0;
	uint8_t port1 = 0;
    
	// Filling rx_loss
	command = Read__Input_Port_1;
	port1 = read_I2C(IOExpanderFor_RXLossOfSignalx, command);  
	
	ExpanderStatesArray[current].rx_loss.Soft_Reset_l = (port1 >> 6) & 0x01;
	
	return ExpanderStatesArray[current].rx_loss.Soft_Reset_l;
}