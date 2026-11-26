#pragma once
#include <signal.h>

#include "SharedMemory.h"
#include "ChannelStatus.h"
#include "interrupts.h"

#define GUI_EXECUTABLE_NAME "Http-App"
#define SHM_NAME "/rfm_shm"  // KM: Shared memory name
#define SHM_SIZE 1241      // KM: max size of the shared memory + 1 byte
#define SEM_NAME "rfm_sem" // KM: Semaphore name



typedef struct
{
	ChannelStatus ChannelStatusArrary[10]; //KM: 24 bytes per cell; 240 total
	unsigned char EncUserName[256];
	unsigned char EncPassword[256];
	char PublicKey[456];
	size_t EncUserNameLength;
	size_t EncPasswordLength;
	size_t PubKeyLength;
	bool SuccessfulLoginFlag;
	int8_t Hubmode;
}SharedMemoryContent;
/*KM: Max size should be 1241 bytes.*/

void init_shared_memory(void); 
void write_shared_memory(void);  
void read_shared_memory(void);
void close_shared_memory(void);  
void update_shared_memory_channel_status(ChannelStatus* channelStatus);
void read_shared_memory_channel_status(ChannelStatus channelStatusArray[10]);
void update_public_key(char* key, size_t keyLength);
unsigned char* get_enc_username();
unsigned char* get_enc_password();
size_t get_enc_username_length(void);
size_t get_enc_password_length(void);
void update_authentication_status(int auth);
void send_signal_to_cli(int signalValue);
void get_gui_process_id(int* id);
