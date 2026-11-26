#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <string.h>

#include "ChannelStatus.h"
#include "LoopSetup.h"
#include "GPIO.h"
#include "util.h"

#include "LED.h"

static const ChannelLEDs ChannelLEDArray[10] = 
{ 
	{ /*TR_0_BP_LED*/{4, 5}, /*TR_0_SD_LED*/{3, 23}, /*TR_0_SY_LED*/{3, 22}},
	{ /*TR_1_BP_LED*/{3, 21}, /*TR_1_SD_LED*/{3, 24}, /*TR_1_SY_LED*/{3, 25}},
	{ /*TR_2_BP_LED*/{3, 26}, /*TR_2_SD_LED*/{0, 14}, /*TR_2_SY_LED*/{4, 13}},
	{ /*TR_3_BP_LED*/{4, 12}, /*TR_3_SD_LED*/{4, 10}, /*TR_3_SY_LED*/{4, 26}},
	{ /*TR_4_BP_LED*/{4, 11}, /*TR_4_SD_LED*/{4, 27}, /*TR_4_SY_LED*/{3, 27}},
	{ /*TR_5_BP_LED*/{4, 8}, /*TR_5_SD_LED*/{4, 9}, /*TR_5_SY_LED*/{3, 9}},
	{ /*TR_6_BP_LED*/{3, 15}, /*TR_6_SD_LED*/{3, 14}, /*TR_6_SY_LED*/{4, 3}},
	{ /*TR_7_BP_LED*/{0, 13}, /*TR_7_SD_LED*/{3, 8}, /*TR_7_SY_LED*/{3, 12}},
	{ /*TR_8_BP_LED*/{0, 1}, /*TR_8_SD_LED*/{0, 8}, /*TR_8_SY_LED*/{2, 14}},
	{ /*TR_9_BP_LED*/{3, 10}, /*TR_9_SD_LED*/{3, 6}, /*TR_9_SY_LED*/{4, 23}},
};

static const LEDInfo miscLEDs[3] =
{
	{4, 29}, // ERROR_LED
	{4, 22}, // RESET_LED
	{3, 13}  // POWER_LED
};

void LEDs_off(void)
{
	LOG_MSG("Turning all LEDs OFF");
	int i = 0;
	

	for (i = 0; i < 10; i++)
	{
		//Turn off Bypass LEDs
		reserve_GPIO(ChannelLEDArray[i].ByPassLEDs.Chipnumber, ChannelLEDArray[i].ByPassLEDs.Bit);
		set_GPIO_value(1);
		release_GPIO();
		
		// Turn off SignalDetect LEDs
		reserve_GPIO(ChannelLEDArray[i].SignalDetectLEDs.Chipnumber, ChannelLEDArray[i].SignalDetectLEDs.Bit);
		set_GPIO_value(1);
		release_GPIO();
		
		//Turn off SyncDetect LEDs
		reserve_GPIO(ChannelLEDArray[i].SyncDetectLEDs.Chipnumber, ChannelLEDArray[i].SyncDetectLEDs.Bit);
		set_GPIO_value(1);
		release_GPIO();
	}
	
	//Turn off Error LED
	reserve_GPIO(miscLEDs[0].Chipnumber, miscLEDs[0].Bit);
	set_GPIO_value(1);
	release_GPIO();
	
	//Turn off Reset LED
	reserve_GPIO(miscLEDs[1].Chipnumber, miscLEDs[1].Bit);
	set_GPIO_value(1);
	release_GPIO();
	
}

void power_LED_on(void)
{
	LOG_MSG("Turning Power LED ON\n");
	reserve_GPIO(miscLEDs[2].Chipnumber, miscLEDs[2].Bit);
	set_GPIO_value(0);
	release_GPIO();
}

void reset_LED_on(void)
{
	LOG_MSG("Turning Reset LED ON\n");
	reserve_GPIO(miscLEDs[1].Chipnumber, miscLEDs[1].Bit);
	set_GPIO_value(0);
	release_GPIO();
}

void reset_LED_off(void)
{
	LOG_MSG("Turning Reset LED ON\n");
	reserve_GPIO(miscLEDs[1].Chipnumber, miscLEDs[1].Bit);
	set_GPIO_value(1);
	release_GPIO();
}

void init_LEDs(void)
{
	LOG_MSG("Initializing LEDs\n");
	power_LED_on();
	
	configure_leds();
}

//OD: Function to compare the bits of the LED variables
bool compare_bits(uint16_t ledsA, uint16_t ledsB, int i)
{
	if ((ledsA & (1 << i)) == (ledsB & (1 << i)))
		return true;
	else
		return false;
}

void configure_leds(void)
{
	LOG_MSG("Configuring LEDs\n");
	ChannelStatus *channel = get_channel_status_array();
	int i = 0;
	uint16_t error_flag = 0;
	uint16_t bypassLED = 0;
	uint16_t signalDetectLED = 0;
	uint16_t syncLED = 0;
	
	//OD: Declare these in a different place?
	static uint16_t prevBypassLED = 0;
	static uint16_t prevSignalDetectLED = 0;
	static uint16_t prevSyncLED = 0;
	static int bypassChange[10] = {0};
	static int signalDetectChange[10] = {0};
	static int syncChange[10] = {0};
	static int LEDStableBuffer = 5; //OD: This variable sets how many polling cycles an LED status must remain stable before it is change
	
	//KM: logic for Bypass LED
	for (i = 0; i < 10; i++)
	{
		
		if (channel[i].Bypass == 0)//auto
		{
			if (((channel[i].Status) & (1 << 6)))//if SD on
			{
				bypassLED &= ~(1 << i); //if SD on, turn bypass led off
			}
			else
			{
				bypassLED |= (1 << i); //if SD off, turn bypass led on
			}
		}
		else if (channel[i].Bypass == 1)
		{
			bypassLED |= (1 << i);
		}
		else if (channel[i].Bypass == 2)
		{
			bypassLED &= ~(1 << i);
		}
		
		if ((channel[i].Status) & (1 << 3) && !(channel[i].Bypass == 2) ) //if channel's tx is disabled, and bypass mode off is not set
		{
			bypassLED |= (1 << i);
		}
		
		if (compare_bits(bypassLED, prevBypassLED, i))
		{
			bypassChange[i]++;
		}
		else
		{
			bypassChange[i] = 0;
		}
	}
	
	//KM: logic for Signal Dectect LED
	for (i = 0; i < 10; i++)
	{
		if (((channel[i].Status) & (1 << 6)))//if SD on
		{
			signalDetectLED |= (1 << i) ; //if SD on, turn SD led on
		}
		else
		{
			signalDetectLED &= ~(1 << i); //if SD off, SD led off
		}
		
		if (compare_bits(signalDetectLED, prevSignalDetectLED, i))
		{
			signalDetectChange[i]++;
		}
		else
		{
			signalDetectChange[i] = 0;
		}
	}
	
	//KM: logic for Sync LED
	for (i = 0; i < 10; i++)
	{
		if (((channel[i].Status) & (1 << 6)) && ((channel[i].Status) & (1 << 5))) //KM: if SD and CDR are postive
		{
			syncLED |= (1 << i);
		}
		else
		{
			syncLED &= ~(1 << i);
		}
		
		if (compare_bits(syncLED, prevSyncLED, i))
		{
			syncChange[i]++;
		}
		else
		{
			syncChange[i] = 0;
		}
	}
	
	//OD: Only change LEDs if it remains the same for back-to-back polls
	for (i = 0; i < 10; i++) 
	{
		if (bypassChange[i] > LEDStableBuffer)
		{		
			//Process the status of the Bypass LED
			if (bypassLED & (1 << i)) //if SD status is on and if the active bit is off
			{
				//Turn Bypass LED on
				reserve_GPIO(ChannelLEDArray[i].ByPassLEDs.Chipnumber, ChannelLEDArray[i].ByPassLEDs.Bit);
				set_GPIO_value(0);
				release_GPIO();
			}
			else 
			{
				//Turn Bypass LED off
				reserve_GPIO(ChannelLEDArray[i].ByPassLEDs.Chipnumber, ChannelLEDArray[i].ByPassLEDs.Bit);
				set_GPIO_value(1);
				release_GPIO();
			}
		}
		
		if (signalDetectChange[i] > LEDStableBuffer)
		{
			//Process the status of the Signal Detect LED
			if (signalDetectLED & (1 << i)) // if TSCVR_SD [X] AND XBAR_SD [X] true
			{
				//Turn Signal Detect LED on
				reserve_GPIO(ChannelLEDArray[i].SignalDetectLEDs.Chipnumber, ChannelLEDArray[i].SignalDetectLEDs.Bit);
				set_GPIO_value(0);
				release_GPIO();
			
			}
			else
			{
				//Turn Signal Detect LED off
				reserve_GPIO(ChannelLEDArray[i].SignalDetectLEDs.Chipnumber, ChannelLEDArray[i].SignalDetectLEDs.Bit);
				set_GPIO_value(1);
				release_GPIO();
			
			}
		}
		
		if (syncChange[i] > LEDStableBuffer)
		{
			//Process the status of the Sync LED
			if (syncLED & (1 << i))// if CDR true
			{
				//Turn Sync LED on
				reserve_GPIO(ChannelLEDArray[i].SyncDetectLEDs.Chipnumber, ChannelLEDArray[i].SyncDetectLEDs.Bit);
				set_GPIO_value(0);
				release_GPIO();
			
			}
			else
			{
				//Turn Sync LED off
				reserve_GPIO(ChannelLEDArray[i].SyncDetectLEDs.Chipnumber, ChannelLEDArray[i].SyncDetectLEDs.Bit);
				set_GPIO_value(1);
				release_GPIO();
			
			}
		}
		
		//process Error LED status
		if (  channel[i].Status & (1 << 1))
		{
			error_flag |= (1 << i);
		}
		else
		{
			error_flag &= ~(1 << i);
		}
		
	}
	
	//Configure Error LED
	if (error_flag > 0)
	{
		//Turn Error LED on
		reserve_GPIO(miscLEDs[0].Chipnumber, miscLEDs[0].Bit);
		set_GPIO_value(0);
		release_GPIO();
	}
	else
	{
		//Turn Error LED off
		reserve_GPIO(miscLEDs[0].Chipnumber, miscLEDs[0].Bit);
		set_GPIO_value(1);
		release_GPIO();
	}
	
	//OD: Set prevLEDs = to currentLEDs
	prevBypassLED = bypassLED;
	prevSignalDetectLED = signalDetectLED;
	prevSyncLED = syncLED;
}


