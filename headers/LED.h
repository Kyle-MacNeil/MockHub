#pragma once

typedef struct
{    
	uint8_t Chipnumber;
	uint8_t Bit;      
}LEDInfo;

typedef struct
{
	LEDInfo ByPassLEDs;
	LEDInfo SignalDetectLEDs;
	LEDInfo SyncDetectLEDs;
		
}ChannelLEDs;


void configure_leds(void);
void power_LED_on(void);
void LEDs_off(void);
void reset_LED_on(void);
void init_LEDs(void);
void reset_LED_off(void);