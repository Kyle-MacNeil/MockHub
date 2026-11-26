#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <openssl/pem.h>
#include <signal.h>

#include "SharedMemory.h"
#include "util.h"
#include "ChannelStatus.h"
#include "LoopSetup.h"

#define CLI_EXECUTABLE_NAME "RFMHUB-CLI"


static int shm_fd = 0;
static SharedMemoryContent *shm_ptr = NULL;
static sem_t *sem = NULL;
static SharedMemoryContent SharedMemoryMessage = { 0 };

void init_shared_memory(void)
{
	LOG_MSG("Initializing shared memory\n");
	int sem_count = 2;
	
	shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); //KM: Opens/creates a shared memory space
	if (shm_fd < 0) {
		LOG_ERROR(CRITICAL_ERROR, "Failed to create shared memory space.");
		//log_error(__func__, "Failed to create shared memory space");
	}
	
	if (ftruncate(shm_fd, SHM_SIZE) < 0) //KM: configures the size of the shared memory space
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to allocate size for shared memory.");
		//log_error(__func__, "Failed to allocate size for shared memory");
	}
	
	shm_ptr = (SharedMemoryContent*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); //KM: map the shared memory space to the shm_ptr pointer
	if (shm_ptr == MAP_FAILED) {
		LOG_ERROR(CRITICAL_ERROR, "failed to map the shared memory to this process.");
		//log_error(__func__, "failed to map the shared memory to this process");
	}
	
	sem = sem_open(SEM_NAME, O_CREAT, 0666, 1); //KM: opens/creates a binary semaphore
	if (sem == SEM_FAILED) {
		LOG_ERROR(CRITICAL_ERROR, "Failed to create semaphore.");
		//log_error(__func__, "Failed to create semaphore");
	}
	
	
	sem_getvalue(sem, &sem_count); //KM: If the process end abrutly sometimes it can mess up the semaphore, therefore I added this bit for debugging
	if (sem_count == 0)
	{
		sem_post(sem);
	}
	
	
}

void write_shared_memory(void)
{
	LOG_MSG("Writing to shared memory\n");
	int sem_count = 0;
	
#ifdef DEBUG
	
	sem_getvalue(sem, &sem_count);
#endif
	
	sem_wait(sem); //KM: decrement the semaphore to prevent race condition (i.e. ensures this process is the only one reading/writing)
	
	memcpy(shm_ptr, &SharedMemoryMessage, sizeof(SharedMemoryContent)); //KM: send data to shared memory
	
	sem_post(sem); //KM: increments the semaphore to tell the CLI/GUI that the shared memory is available again
	
	sem_getvalue(sem, &sem_count);//KM: check the sem count before moving on
	if (sem_count == 0)
	{
		sem_post(sem);
		
	}
	
}

void read_shared_memory(void)
{
	LOG_MSG("Reading from shared memory\n");
	int sem_count = 0;
	
#ifdef DEBUG
	
	sem_getvalue(sem, &sem_count);
#endif
	
	sem_wait(sem);

	memcpy(&SharedMemoryMessage, shm_ptr, sizeof(SharedMemoryContent));

	sem_post(sem);
	
	sem_getvalue(sem, &sem_count); //KM: check the sem count before moving on
	if (sem_count == 0)
	{
		sem_post(sem);
		
	}
	
}

void close_shared_memory(void)
{	
	LOG_MSG("Closing shared memory\n");
	munmap(shm_ptr, SHM_SIZE);
	close(shm_fd);
	sem_close(sem);
	sem_unlink(SEM_NAME);
}

void update_shared_memory_channel_status(ChannelStatus* channelStatus)
{
	LOG_MSG("Updating shared memory channel status\n");
	memcpy(SharedMemoryMessage.ChannelStatusArrary, channelStatus, sizeof(ChannelStatus) * 10);
	
	SharedMemoryMessage.Hubmode = get_hub_mode();
	
	write_shared_memory();
}

void read_shared_memory_channel_status(ChannelStatus channelStatusArray[10])
{
	LOG_MSG("Reading shared memory channel status\n");
	read_shared_memory();
	
	memcpy(channelStatusArray, SharedMemoryMessage.ChannelStatusArrary, (sizeof(ChannelStatus) * 10));
	
}

void update_public_key(char* key, size_t keyLength)
{
	LOG_MSG("Updating public key\n");
	int keyLengthCheck = strlen(key);
	
	if (keyLength != keyLengthCheck)
	{
		LOG_ERROR(CRITICAL_ERROR, "Error: public key length discrepency.");
		//log_error(__func__, "Error: public key length discrepency");
	}
	
	strncpy(SharedMemoryMessage.PublicKey, key, keyLength);
	
	SharedMemoryMessage.PubKeyLength = keyLength;
	
	write_shared_memory();
}

unsigned char* get_enc_username()
{
	LOG_MSG("Getting encrypted username\n");
	return SharedMemoryMessage.EncUserName;
}

unsigned char* get_enc_password()
{
	LOG_MSG("Getting encrypted password\n");
	return SharedMemoryMessage.EncPassword;
}

size_t get_enc_username_length(void)
{
	LOG_MSG("Getting encrypted username length\n");
	return SharedMemoryMessage.EncUserNameLength;
}

size_t get_enc_password_length(void)
{
	LOG_MSG("Getting encrypted password length\n");
	return SharedMemoryMessage.EncPasswordLength;
}

void get_CLI_PID(int* pidCLI)
{
	LOG_MSG("Getting CLI pid\n");
	char commandline[32];
	
	//KM: Finds the newest PID with the RFM Hubs main process exectuable name
	snprintf(commandline, sizeof(commandline), "pgrep -n %s", CLI_EXECUTABLE_NAME); 
	
	FILE *fp = popen(commandline, "r"); //KM: open pipe to run the command and capture its output
	if (fp == NULL)
	{
		//KM: error
		perror("popen failed");
		LOG_MSG("popen failed for getting the CLI process id\n");

	}
	
	//KM: read the captured output of the command
	if (fscanf(fp, "%d", pidCLI) != 1) {
		//KM: error
		perror("no PID found");
		LOG_MSG("CLI Process id is not found\n");
	}
	
	pclose(fp); //KM: close the pipe
	
}

void send_signal_to_cli(int signalValue)
{
	LOG_MSG("Sendign signal to CLI/GUI\n");
	int pidCLI = -1; //KM: initialize as an invalid number
	
	get_CLI_PID(&pidCLI);
	
	//KM: check to see if pid is a valid #
	if (pidCLI <= 0)
	{
		//log_error(__func__, "Error: Invlaid PID");
		LOG_ERROR(CRITICAL_ERROR, "Error: Invlaid PID.");
	}
	//KM: we already use the SIGTRMIN signal, therefore we incermented this signal ID by +1
	if (kill(pidCLI, signalValue) == -1) 
	{
		//log_error(__func__, "Failed to send cli RT signal to the main process");
		LOG_ERROR(CRITICAL_ERROR, "Failed to send cli RT signal to the main process.");
	}
}

void update_authentication_status(int retval)
{
	LOG_MSG("Updating authentication status\n");
	bool auth = 0;
	
	if (retval == 0)
	{
		//KM: successful authentication
		auth = 1;
	}
	else if (retval != 0)
	{
		//KM: failed authentication
		auth = 0;
	}
	
	SharedMemoryMessage.SuccessfulLoginFlag = auth;
	
	write_shared_memory();
	
}

void get_gui_process_id(int* id)
{
	LOG_MSG("Getting GUI Process id\n");
	char commandline[32];
	
	//KM: Finds the newest PID with the RFM Hubs main process exectuable name
	snprintf(commandline, sizeof(commandline), "pgrep -n %s", GUI_EXECUTABLE_NAME); 
	
	FILE *fp = popen(commandline, "r"); //KM: open pipe to run the command and capture its output
	if (fp == NULL)
	{
		//KM: error
		perror("popen failed");
		LOG_MSG("popen failed to get GUI process id\n");
	}
	
	//KM: read the captured output of the command
	if (fscanf(fp, "%d", id) != 1) {
		//KM: error
		perror("no PID found");
		LOG_MSG("GUI process id is not found\n");
	}
	
	pclose(fp); //KM: close the pipe
}