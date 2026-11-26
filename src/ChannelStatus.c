/* This file contains a database of the current configuration of the 
Xbar and GPIO expanders (i.e. the configuration of the RFM Hub) */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/reboot.h>

#include "GPIOExpander.h"
#include "LoopSetup.h"
#include "ChannelStatus.h"
#include "SharedMemory.h"
#include "interrupts.h"
#include "XBar.h"
#include "util.h"
#include "LED.h"
#include "login.h"
#include "network.h"
#include "interrupts.h"

//KM: 150 bytes is the size of a 10 cell struct ChannelStatus variable type
#define CONFIG_FILE_SIZE 150
#define CONFIG_FILE_PATH "/etc/rfmhub.conf"

#define IF_TX_FAULT_ACTIVE ((*tx_fault_ptr) & (1 << i) & (!(( (*mod_absent_ptr) & (1 << i) ))))
#define IF_SD_XBAR_SD_GPIO_CONFLICT (((~(*rx_loss_ptr)) & (1 << i)) ^ (SD_present & (1 << i)))
#define IF_MOD_ABS_AND_SD_PRESENT_CONFLICT (((*mod_absent_ptr) & (1 << i)) & (SD_present & (1 << i)))
#define IF_CDR_LOCK_0N_FOR_CH_i (currentCDR & (1<<i))

static int restartInterruptOverflowCoutner = 0;

static ChannelStatusArrays ChannelStatusArray = { 0 };

static uint16_t SignalDetectStatus = 0;

static int setDefaultToggleCount= 0;


//KM: funciton prototypes here
void handle_other_bit_change(uint64_t bit_index);
void init_update_channel_status(ExpanderStates gpiostate, XBarStates xbarstate);
void init_read_config(void);
void write_config(void);

void init_update_channel_array(void)
{
	LOG_MSG("Initialize and update channel array\n");
	ExpanderStates* currentGPIOExpanderState = init_get_GPIO_expander_state();
	XBarStates* currentXbarState = init_get_XBar_state();
	
	init_update_channel_status(*currentGPIOExpanderState, *currentXbarState);
}

void save_state(void)
{
	LOG_MSG("Saving state\n");
	ExpanderStates* expanderState = save_state_GPIO_expander();
	
	XBarStates* xbarState = save_state_xbar();
	
	update_channel_status(*expanderState, *xbarState);
}

void init_update_channel_status(ExpanderStates gpiostate, XBarStates xbarstate)
{
	LOG_MSG("Initialize and update channel status\n");
	int i = 0;
	
	//KM: GPIO Expander info
	uint16_t* rx_loss_ptr = (uint16_t*) &gpiostate.rx_loss;
	uint16_t* tx_fault_ptr = (uint16_t*) &gpiostate.tx_fault;
	uint16_t* tx_disable_ptr = (uint16_t*) &gpiostate.tx_disable;
	uint16_t* mod_absent_ptr = (uint16_t*) &gpiostate.mod_absent;
	
	//KM: Xbar info
	uint16_t SD_present = xbarstate.XbarSD;
	uint16_t CDR_present = xbarstate.XbarCDR;
	
	//KM: Save the previous channels states
	memcpy(ChannelStatusArray.PreviousChannelStatusArray, ChannelStatusArray.CurrentChannelStatusArray, (sizeof(ChannelStatus) * 10));
	
	for (i = 0; i < 10; i++)
	{
		//Set Xbar SD 
		//NOTE!!! since the XBar SD is not working, this is acutally the module SD (i.e. bits 7 & 6 are the same)
		if (SD_present & (1 << i))
		{
			//Xbar SD on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 7);
		}
		else
		{
			//Xbar SD off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 7);
		}
		
		//Set module SD
		if ((~(*rx_loss_ptr)) & (1 << i))
		{
			//Module SD on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 6);
		}
		else
		{
			//Module SD off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 6);
		}
	
		//Set Xbar CDR
		if ((CDR_present & (1 << i)))
		{
			//CDR on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 5);
		}
		else
		{
			//CDR off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 5);
		}
		
		//Set Module Absence
		if ((*mod_absent_ptr) & (1 << i))
		{
			//Module Absence on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 4);
			
		}
		else
		{
			//Module Absence off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 4);
		}
		
		//Set TX disable
		if ((*tx_disable_ptr) & (1 << i))
		{
			//TX disable on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 3);
		}
		else
		{
			//TX disable off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 3);
		}
		
		//Set TX Fault
		if ((*tx_fault_ptr) & (1 << i))
		{
			//TX Fault on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 2);
		}
		else
		{
			//TX Fault off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 2);
		}

		//KM: Set Fault detected bit and Active bit   
		if (IF_TX_FAULT_ACTIVE || IF_SD_XBAR_SD_GPIO_CONFLICT || IF_MOD_ABS_AND_SD_PRESENT_CONFLICT )
		{
			//Fault bit on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 1);
			
			//Active bit off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 0);
		}
		else
		{
			//Fault bit off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 1);
			
			//Active bit on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 0);
		}
		
		
		//KM: after setting the active bit based on the faults, check to see if the channel is inactive for other reasons other than a fault, such as tx disable or mod absence
		if ((*tx_disable_ptr) & (1 << i) || ((*mod_absent_ptr) & (1 << i)))
		{
			//Active bit off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 0);
		}
			
	}
	
	//KM: write to local config file to save configuration in event of shut off
	write_config();
	
	update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
	
}

void update_channel_status(ExpanderStates gpiostate, XBarStates xbarstate)
{
	LOG_MSG("Updating channel status\n");
	int i = 0;
	
	//KM: GPIO Expander info
	uint16_t* rx_loss_ptr = (uint16_t*) &gpiostate.rx_loss;
	uint16_t* tx_fault_ptr = (uint16_t*) &gpiostate.tx_fault;
	uint16_t* tx_disable_ptr = (uint16_t*) &gpiostate.tx_disable;
	uint16_t* mod_absent_ptr = (uint16_t*) &gpiostate.mod_absent;
	
	//KM: Xbar info
	uint16_t SD_present = xbarstate.XbarSD;
	uint16_t CDR_present = xbarstate.XbarCDR;
	
	//KM : get the current channel status array from the shared memory, in case changes have been made by cli
	read_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
	
	//KM: Save the previous channels states
	memcpy(ChannelStatusArray.PreviousChannelStatusArray, ChannelStatusArray.CurrentChannelStatusArray, (sizeof(ChannelStatus) * 10));
	
	for (i = 0; i < 10; i++)
	{
		//Set Xbar SD 
		//NOTE!!! since the XBar SD is not working, this is acutally the module SD
		if (SD_present & (1 << i))
		{
			//Xbar SD on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 7);
		}
		else
		{
			//Xbar SD off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 7);
		}
		
		//Set module SD
		if ( (~(*rx_loss_ptr)) & (1 << i) )
		{
			//Module SD on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 6);
		}
		else
		{
			//Module SD off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 6);
		}
	
		//Set Xbar CDR
		if ( (CDR_present & (1 << i)) )
		{
			//CDR on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 5);
		}
		else
		{
			//CDR off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 5);
		}
		
		//Set Module Absence
		if ( (*mod_absent_ptr) & (1 << i))
		{
			//Module Absence on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 4);
			
		}
		else
		{
			//Module Absence off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 4);
		}
		
		
		//Set TX Fault
		if ((*tx_fault_ptr) & (1 << i))
		{
			//TX Fault on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 2);
		}
		else
		{
			//TX Fault off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 2);
		}

		
		//KM: Set Fault detected bit and Active bit
		if (IF_TX_FAULT_ACTIVE || IF_SD_XBAR_SD_GPIO_CONFLICT || IF_MOD_ABS_AND_SD_PRESENT_CONFLICT )
		{
			//Fault bit on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 1);
			
			//Active bit off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 0);
		}
		else
		{
			//Fault bit off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 1);
			
			//Active bit on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 0);
		}
		
		
		//KM: after setting the active bit based on the faults, check to see if the channel is inactive for other reasons other than a fault, such as tx disable or mod absence
		if ( ((*tx_disable_ptr) & (1 << i)) || ((*mod_absent_ptr) & (1 << i)) )
		{
			//Active bit off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 0);
		}
		
		
	}
	
	//KM: write to local config file to save configuration in event of shut off
	write_config();
	
	update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
	
}

void update_CDR_channel_status(uint16_t currentCDR)
{
	//KM: Save the previous channels states
	memcpy(ChannelStatusArray.PreviousChannelStatusArray, ChannelStatusArray.CurrentChannelStatusArray, (sizeof(ChannelStatus) * 10));
	
	for (int i = 0; i < 10; i++)
	{
		if (IF_CDR_LOCK_0N_FOR_CH_i)
		{
			//KM: Turn CDR status for CH i ON
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 5);
		}
		else
		{
			//KM: Turn CDR status for CH i OFF
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 5);
		}
	}

	
}

ChannelStatus* get_channel_status_array(void)
{
	LOG_MSG("Get channel status array\n");
	return ChannelStatusArray.CurrentChannelStatusArray;
}

void process_cli_command(void)
{
	LOG_MSG("Processing CLI command\n");
	//KM: Save the previous channels states
	memcpy(ChannelStatusArray.PreviousChannelStatusArray, ChannelStatusArray.CurrentChannelStatusArray, (sizeof(ChannelStatus) * 10));
	
	read_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
	
	start_re_configuration();
	
	update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
	
}

void process_gui_command(void)
{
	LOG_MSG("Processing GUI command\n");
	//KM: Save the previous channels states
	memcpy(ChannelStatusArray.PreviousChannelStatusArray, ChannelStatusArray.CurrentChannelStatusArray, (sizeof(ChannelStatus) * 10));
	
	read_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
	
	LOG_MSG("Starting re config\n");
	start_re_configuration();
	
	update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
}

void set_default_settings(void)
{
	int i = 0;
	
	for (i = 0; i < 10; i++)
	{
		//KM: bypass set to auto
		ChannelStatusArray.CurrentChannelStatusArray[i].Bypass = 0;
		
		//KM:Dark on Dark set to off
		ChannelStatusArray.CurrentChannelStatusArray[i].DarkOnDark = 0;
		
		//KM:TX is enabled
		ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1<<3);
		
	}
	//KM: set password to default password
	change_p_default();
	
	erase_config();
	
	
	
	update_default_ip();
	/*
	//KM: send and wait for the GUI signal here for the GUI changes
	send_signal_to_gui(SIGNAL_DEFAULT_GUI_SETTINGS);
	
	wait_for_verification_signal(GUI_DONE);
	*/
	
	start_re_configuration();
	
	update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
	
	//KM: After reconfiguration, flash the restart LED 5 times to show that the changes have been made
	for (int j = 0; j < 4; j++)
	{
		sleep(1);
		reset_LED_on();
				
		sleep(1);
				
		reset_LED_off();
	}
}

void update_channel_diagnostics(uint64_t changedBitsGPIO, uint64_t changedBitsCDR)
{
	struct timespec ts;
	long milliseconds = 0;
	
	clock_gettime(CLOCK_REALTIME, &ts);
	
	//KM: convert the time given (i.e. seconds and nano seconds) and convert into a single miliseconds timer
	//Note: the timestap is taken in reference to Janurary 1st 1970. The long variable will expire in 292 million years from that date i.e. practically forever
	milliseconds = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	
	if ( changedBitsGPIO & 0x0001000100010001 || changedBitsCDR & 0x0001)
	{
		//KM: Channel 0  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ( (milliseconds - ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[0].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[0].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[0].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[0].Inttimestamp = milliseconds;
			}
		}
		
	}
	
	if (changedBitsGPIO & 0x0002000200020002 || changedBitsCDR & 0x0002)
	{
		//KM: Channel 1  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[1].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[1].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[1].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[1].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0004000400040004 || changedBitsCDR & 0x0004)
	{
		//KM: Channel 2  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[2].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[2].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[2].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[2].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0008000800080008 || changedBitsCDR & 0x0008)
	{
		//KM: Channel 3  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[3].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[3].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[3].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[3].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0010001000100010 || changedBitsCDR & 0x0010)
	{
		//KM: Channel 4  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[4].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[4].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[4].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[4].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0020002000200020 || changedBitsCDR & 0x0020)
	{
		//KM: Channel 5  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[5].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[5].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[5].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[5].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0040004000400040 || changedBitsCDR & 0x0040)
	{
		//KM: Channel 6  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[6].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[6].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[6].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[6].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0080008000800080 || changedBitsCDR & 0x0080)
	{
		//KM: Channel 7  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[7].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[7].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[7].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[7].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0100010001000100 || changedBitsCDR & 0x0100)
	{
		//KM: Channel 8  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[8].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[8].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[8].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[8].Inttimestamp = milliseconds;
			}
		}
	}
	
	if (changedBitsGPIO & 0x0200020002000200 || changedBitsCDR & 0x0200)
	{
		//KM: Channel 9  counter
		if (ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp == 0)
		{
			//KM: first interrupt for channel
			ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp = milliseconds;
			/*KM: Note, no counter is incremented for the first interrupt of the channel since there is no previous interrupt
			 * to categlorize its frequency (i.e. if the last interrupt was with 1 sec, 1 min, or 1 hour)
			 **/
		}
		else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp) <= 0)
		{
			//KM: Error - invalid timestamp
			LOG_ERROR(CRITICAL_ERROR, "Error: invalid time stamp recorded");
		}
		else
		{
			//KM: interrupt occured less than a second from the last interrupt
			if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp) < 1000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[9].Smallcount++;
				ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than a minute from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp) < 60000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[9].Midcount++;
				ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp = milliseconds;
			}
			//KM: interrupt occured less than an hour from the last interrupt
			else if ((milliseconds - ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp) < 3600000)
			{
				ChannelStatusArray.CurrentChannelStatusArray[9].Longcount++;
				ChannelStatusArray.CurrentChannelStatusArray[9].Inttimestamp = milliseconds;
			}
		}
	}
	
	update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
}

void process_states(void) 
{
	LOG_MSG("Process states\n");
	uint64_t changedBitsGPIO = compare_GPIO_states();
	uint64_t changedBitsCDR = compare_CDR_states();

	if (changedBitsGPIO != 0 || changedBitsCDR != 0) 
	{
		// If any bits change i.e. HW change
		
		if (get_reset_pin_state() == 0)
		{
			/*KM: check if the Hub is stuck in a loop. Only needs 5 cycles to trigger default reset, 
			*but added an extra 6 in case there are some other HW interrupt in between toggling the dual/single switch
			*/
			restartInterruptOverflowCoutner++;
			if (restartInterruptOverflowCoutner > 10)
			{
				reset_LED_on();
				
				sleep(2);
				
				if (reboot(RB_AUTOBOOT) == -1) 
				{
					//error
					LOG_ERROR(CRITICAL_ERROR, "Failed to reboot");
				}
				return;
			}
		}
	        
	    // Check if the bit belongs to the specified list of bits
		if (changedBitsGPIO & 0x3FF03FF03FF03FF || changedBitsCDR & 0x3FF) 
		{ 
			//KM: check which channels have had changes and incrementent their counter and set time stamp
			update_channel_diagnostics(changedBitsGPIO, changedBitsCDR);
			
			start_re_configuration();
			
			update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
		} 
	        
		if (changedBitsGPIO & 0xFC00FC00FC00FC00)//KM: Actions that are not a re-configuration (i.e. setting changes and restart button)
		{
		        
			handle_other_bit_change(changedBitsGPIO);
		}
		
	}
}

void handle_other_bit_change(uint64_t bit_index) {
	LOG_MSG("Handle other bit change\n");
	
	if (bit_index & 0x0000000000004000)
	{
		printf("Soft_Reset_l changed.\n");
		LOG_MSG("Soft_Reset_l changed.\n");
		syslog(LOG_INFO, "Soft_Reset_l changed.\n");
		uint16_t restartBit = get_reset_pin_state();
	    
		sync();
		
		//KM: check if the reset pin is asserted 1 or 0. 0 = reset; 1 = do not reset
		if (restartBit == 0)
		{
			begin_reset_response();
		}
		else if (restartBit == 1)
		{
			//KM: after the reset button is let go, reset the defaule setting toggle counter
			setDefaultToggleCount = 0;
			restartInterruptOverflowCoutner = 0;
		}
		else
		{
			//KM: Error
			LOG_ERROR(CRITICAL_ERROR, "Error: reading an invalid reset status value");
		}

	}
	
	if (bit_index & 0x0000000000008000)
	{
		printf("S_D_Sel changed.\n");
		LOG_MSG("S_D_Sel changed.\n");
		uint16_t restartBit = get_reset_pin_state();
		
		if (restartBit == 1)
		{
			// Add handling code for S_D_Sel
			change_hub_mode();
			start_re_configuration();
			update_shared_memory_channel_status(ChannelStatusArray.CurrentChannelStatusArray);
		}
		else if ( restartBit == 0)
		{
			setDefaultToggleCount++;
			//KM: check if gang switch has been toggled 5 times
			if (setDefaultToggleCount > 4)
			{
				//KM: after switch has been toggled 5 times, set default settings
				restartInterruptOverflowCoutner = 0;
				
				set_default_settings();
			}
		}
		else
		{
			//KM: Error
			LOG_ERROR(CRITICAL_ERROR, "Error: reading an invalid reset status value");
		}
		
	}
	
	if (bit_index & 0x0000100000000000)
	{
		printf("EEProm_wr_l changed.\n");
		LOG_MSG("EEProm_wr_l changed.\n");
		// Add handling code for EEProm_wr_l
	}
	
	if (bit_index & 0x0000200000000000)
	{
		printf("MUX_RST changed.\n");
		LOG_MSG("MUX_RST changed.\n");
		// Add handling code for MUX_RST
	}
	
	if (bit_index & 0x0000400000000000)
	{
		printf("TR_RS0 changed.\n");
		LOG_MSG("TR_RS0 changed.\n");
		// Add handling code for TR_RS0
	}
	
	if (bit_index & 0x0000800000000000)
	{
		printf("TR_RS1 changed.\n");
		LOG_MSG("TR_RS1 changed.\n");
		// Add handling code for TR_RS1
	}
	
	if (!(bit_index & 0x0000F000000000C0))
	{
		printf("Unknown bit index %d changed.\n", (int) bit_index);
		LOG_MSG("Unknown bit index %d changed.\n", (int) bit_index);
	}
	
}

uint16_t get_TXstatus(void)
{
	LOG_MSG("Get TX status\n");
	int i = 0;
	uint16_t TXstatus = 0;
	uint16_t mask = (1 << 3); //KM: Bit 3 in the 16bit status variable represents if TX is disabled or not
	
	for (i = 0; i < 10; i++)
	{
		if ((ChannelStatusArray.CurrentChannelStatusArray[i].Status & mask) > 0)
		{
			TXstatus |= (1 << i); //KM: TX disable for channel i is on
		}
		else if ((ChannelStatusArray.CurrentChannelStatusArray[i].Status & mask) == 0)
		{
			TXstatus &= ~(1 << i); //KM: TX disable for channel i is off
		}
	}
	
	return TXstatus;
}

void get_bypass_status(uint8_t bypassStatusArray[])
{
	LOG_MSG("Get bypass status\n");
	int i = 0;
	
	for (i = 0; i < 10; i++)
	{
		bypassStatusArray[i] = ChannelStatusArray.CurrentChannelStatusArray[i].Bypass;
	}
}


uint16_t get_dark_on_dark_status(void)
{
	LOG_MSG("Get dark on dark status\n");
	int i = 0;
	uint16_t darkOnDarkStatus = 0;
	
	for (i = 0; i < 10; i++)
	{
		if (ChannelStatusArray.CurrentChannelStatusArray[i].DarkOnDark == 1)
		{
			darkOnDarkStatus |= (1 << i); //KM: Dark on Dark for channel i is on
		}
		else if (ChannelStatusArray.CurrentChannelStatusArray[i].DarkOnDark == 0)
		{
			darkOnDarkStatus &= ~(1 << i); //KM: Dark on Dark for channel i is off
		}
		else 
		{
			LOG_ERROR(CRITICAL_ERROR, "Failed to read the Dark on Dark status.");
			//log_error(__func__,"Failed to read the Dark on Dark status");
		}
	}
	
	return darkOnDarkStatus;
}

uint16_t get_fault_status(void)
{
	LOG_MSG("Get fault status\n");
	uint16_t faultStatus = 0;
	uint8_t mask = (1 << 1);//KM: the fault detect bit is the bit at position 1 (i.e. 2nd least signifcant bit)
	int i = 0;
	
	for (i = 0; i < 10; i++)
	{
		
		if ((ChannelStatusArray.CurrentChannelStatusArray[i].Status & mask) > 0)//KM: if a fault is detected on channel i
		{
			faultStatus |= (1 << i); //KM: set bit i to 1 to indicate that channel i has a fault
		}
		else
		{
			faultStatus &= ~(1 << i);//KM: set bit i to 0 to indicate that channel i does not have fault
		}
		
	}
		
	return faultStatus;
}

void update_TXstatus(uint16_t tx_Status)
{
	LOG_MSG("Update transmission status\n");
	int i = 0;

	for (i = 0; i < 10; i++)
	{
		if (((tx_Status & (1 << i))) > 0)
		{
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 3);
		}
		else
		{
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 3);
		}
		
	}
}

void update_signal_detect_status(uint16_t sdStatus)
{
	LOG_MSG("Update signal detect status\n");
	SignalDetectStatus = sdStatus;
}

uint16_t get_sd_status()
{
	LOG_MSG("Get Signal detect status\n");
	return SignalDetectStatus;
}

void init_read_config(void) 
{
	size_t bytesRead = 0;
	char buffer[CONFIG_FILE_SIZE + 1]; 
	FILE *fp = fopen(CONFIG_FILE_PATH, "r");
	
	if (fp == NULL) 
	{
		LOG_ERROR(CRITICAL_ERROR, "Error read configuration settings");
	}
	
	
	// KM: Check if the config file is empty
	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	if (fileSize == 0) 
	{
		LOG_MSG("Config file is empty.\n");
		fclose(fp);
		return;
	}
	
	//KM: Reset file pointer to the beginning before reading
	fseek(fp, 0, SEEK_SET);
	
	bytesRead = fread(ChannelStatusArray.CurrentChannelStatusArray, sizeof(ChannelStatus), 10, fp);

	if (bytesRead != 10) 
	{
		LOG_ERROR(CRITICAL_ERROR, "Error read configuration settings");
	}
	

	fclose(fp);
}

void write_config(void) 
{
	FILE *fp = fopen(CONFIG_FILE_PATH, "wb");
	
	if (!fp) 
	{
		LOG_ERROR(CRITICAL_ERROR, "Error write configuration settings");
	}

	size_t written = fwrite(ChannelStatusArray.CurrentChannelStatusArray, sizeof(ChannelStatus), 10, fp);
	if (written != 10) 
	{
		LOG_ERROR(CRITICAL_ERROR, "Error write configuration settings");
	}

	fclose(fp);
}

void erase_config(void)
{
	FILE *fp = fopen(CONFIG_FILE_PATH, "w");
	//KM: opening the file without writting to it will truncate and erase it contents
	
	if (!fp) 
	{
		LOG_ERROR(CRITICAL_ERROR, "Error write configuration settings");
	}

	fclose(fp);
}

void update_valid_channels(void)
{
	int i = 0;
	uint8_t mask = 0b11110;
	//KM: the mask checks if the MOD_ABS, TX_DIS, TX_FLT, and the fault bit is asserted 1 or 0
	
	for (i = 0; i < 10; i++)
	{
		//KM: if tx disable or module absence is off, then set the active bit to 0 (to indicate the channel is not active)
		if (ChannelStatusArray.CurrentChannelStatusArray[i].Status & mask)
		{
			//Active bit off
			ChannelStatusArray.CurrentChannelStatusArray[i].Status &= ~(1 << 0);
		}
		else
		{
			//Active bit on
			ChannelStatusArray.CurrentChannelStatusArray[i].Status |= (1 << 0);	
		}
	}
	
}

void poll_CDR(void)
{
	int i = 0;
	uint16_t currentCDR = 0;
	
	currentCDR = read_CDR_status();
	
	
	for (i = 0; i < 10; i++)
	{
		if ( ((ChannelStatusArray.CurrentChannelStatusArray[i].Status & (1 << 5))>>5) ^ ((currentCDR & (1 << i))>>i) )
		{
			CDR_poll_response();
			
			return;
		}
	}

}



void update_CDR_status(void)
{
	uint16_t currentCDR = 0;
	
	currentCDR = read_CDR_status();
	
	update_CDR_channel_status(currentCDR);
}

uint16_t get_CDR_status()
{
	int i = 0;
	static uint16_t CDR_status = 0;
	
	for (i = 0; i < 10; i++)
	{
		if ((ChannelStatusArray.CurrentChannelStatusArray[i].Status & (1 << 5)))
		{
			CDR_status |= (1 << i);
		}
	}
	
	return CDR_status;
	
}