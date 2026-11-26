#pragma once

#include "XBar.h"

#include "GPIOExpander.h"

#include "ChannelStatus.h"
#include "logging.h"

typedef struct 
{
	/*Channel Instability Counters.

	Counts the number of interrupts occurring for an individual channel
	and seperates the count based on how much time has past since
	the previous interrupt */
	uint8_t Longcount; // > 1 minute
	uint8_t Midcount; // < 1 minute
	uint16_t Smallcount; // < 1 second

	//Timestamp of channel interrupt
	long Inttimestamp;
	
	/* KM:
	 *	For the Bypass byte
	 *	0 = Bypass is auto
	 *	1 = Bypass is on
	 *	2 = Bypass is off
	 **/
	uint8_t Bypass;
	
	/* KM:
	 * For the DarkOnDark byte
	 * 0 = DarkOnDark mode is off
	 * 1 = DarkOnDark mode is on
	 **/
	uint8_t DarkOnDark;

	/*Status Byte
	Each bit in this byte represents an individual status aspect
	pertaining to an individual channel.
	See what each bit represents below
	7 - Signal detect from Xbar 
	6 - Signal detect from GPIO expander (i.e. RX_LOSS, which is the inverse of SD detect)
	5 - CDR Lock from Xbar
	4 - MOD_ABS from GPIO expander
	3 - TX_DIS from GPIO expander
	2 - TX_FLT from GPIO expander
	1 - Faults detected (i.e. detection of unexpected behaviour, such as discrepancies between Xbar SD and Module SD, TX_FLT)
	0 - Active (i.e. are subroutines still runing? is CDR locked? is SD present?, etc)
	*/
	uint8_t Status;

} ChannelStatus;

typedef struct
{
	ChannelStatus CurrentChannelStatusArray[10];
	
	ChannelStatus PreviousChannelStatusArray[10];
	
}ChannelStatusArrays;



void init_update_channel_array(void);
void update_channel_status(ExpanderStates gpiostate, XBarStates xbarstate);
ChannelStatus* get_channel_status_array(void);
void process_cli_command(void);
void process_gui_command(void);
void save_state(void);
void process_states(void);
uint16_t get_TXstatus(void);
void get_bypass_status(uint8_t bypassStatusArray[]);
uint16_t get_dark_on_dark_status(void);
uint16_t get_fault_status(void);
void update_TXstatus(uint16_t tx_Status);
void update_signal_detect_status(uint16_t sdStatus);
uint16_t get_sd_status();
void init_read_config(void);
void update_valid_channels(void);
void erase_config(void);
void poll_CDR(void);
void update_CDR_channel_status(uint16_t currentCDR);
void update_CDR_status(void);