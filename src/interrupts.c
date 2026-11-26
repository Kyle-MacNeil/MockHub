#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>

#include "interrupts.h"
#include "util.h"
#include "GPIOExpander.h"
#include "LoopSetup.h"
#include "LED.h"
#include "ChannelStatus.h"
#include "rfmhub.h"
#include "SharedMemory.h"
#include "crypto.h"
#include "login.h"


void disable_interrupt_repsonses(void)
{
	sigset_t set;
	
	//KM: Initialize a signal set
	if (sigemptyset(&set) == -1) 
	{
		LOG_MSG("ERROR: Initialization of interrupt blocking failed\n");
		return;
	}
	
	//KM: Add the signals to block (disable)
	if (sigaddset(&set, HW_INTTERUPT_SIGNAL) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named HW_INTTERUPT_SIGNAL \n");
		return;
	}
	if (sigaddset(&set, CLI_SETTING_CHANGE_SIGNAL) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named CLI_SETTING_CHANGE_SIGNAL \n");
		return;
	}
	if (sigaddset(&set, CLI_PROCESS_LOGIN_VERIFICATION_SIGNAL) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named CLI_PROCESS_LOGIN_VERIFICATION_SIGNAL \n");
		return;
	}
	if (sigaddset(&set, CLI_PROCESS_NEW_PASSWORD) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named CLI_PROCESS_NEW_PASSWORD \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_CONFIG_CHANGE_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named SIGNAL_CONFIG_CHANGE_GUI \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_HEARTBEAT_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named SIGNAL_HEARTBEAT_GUI \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_VERIFY_GUI_LOGIN) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named SIGNAL_VERIFY_GUI_LOGIN \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_CHANGE_PASSWORD_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named SIGNAL_CHANGE_PASSWORD_GUI \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_SLEEP_FROM_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to block POSIX signal named SIGNAL_SLEEP_FROM_GUI \n");
		return;
	}
	
	//KM: block the sigals in the specified in the set
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) 
	{
		LOG_MSG("ERROR: sigprocmask SIG_BLOCK failed to disable POSIX singal interrupts\n");
		return;
	}
}

void enable_interrupt_repsonses(void)
{
	sigset_t set;
	
	// KM: Initialize a signal set
	if (sigemptyset(&set) == -1) 
	{
		LOG_MSG("ERROR: Initialization of interrupt unblocking failed\n");
		return;
	}
	
	// KM: Add the signals to unblock (enable)
	if (sigaddset(&set, HW_INTTERUPT_SIGNAL) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named HW_INTTERUPT_SIGNAL \n");
		return;
	}
	if (sigaddset(&set, CLI_SETTING_CHANGE_SIGNAL) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named CLI_SETTING_CHANGE_SIGNAL \n");
		return;
	}
	if (sigaddset(&set, CLI_PROCESS_LOGIN_VERIFICATION_SIGNAL) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named CLI_PROCESS_LOGIN_VERIFICATION_SIGNAL \n");
		return;
	}
	if (sigaddset(&set, CLI_PROCESS_NEW_PASSWORD) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named CLI_PROCESS_NEW_PASSWORD \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_CONFIG_CHANGE_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named SIGNAL_CONFIG_CHANGE_GUI \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_HEARTBEAT_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named SIGNAL_HEARTBEAT_GUI \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_VERIFY_GUI_LOGIN) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named SIGNAL_VERIFY_GUI_LOGIN \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_CHANGE_PASSWORD_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named SIGNAL_CHANGE_PASSWORD_GUI \n");
		return;
	}
	if (sigaddset(&set, SIGNAL_SLEEP_FROM_GUI) == -1) 
	{
		LOG_MSG("ERROR: Failed to unblock POSIX signal named SIGNAL_SLEEP_FROM_GUI \n");
		return;
	}
	
	// KM: Unblock the signals specified in the set.
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) 
	{
		LOG_MSG("ERROR: sigprocmask SIG_UNBLOCK failed to enable POSIX signal interrupts\n");
		return;
	}

}

void CDR_poll_response(void)
{
	LOG_MSG("start CDR poll repsonse\n");
	syslog(LOG_INFO, "start CDR poll repsonse\n");
	
	if (get_init_flag() == 1) 
	{
		save_state();
	
		process_states();
	}
	else
	{
		LOG_MSG("Interrupt is too early, initialization is not complete\n");
		syslog(LOG_INFO, "Interrupt is too early, initialization is not complete");
	}
}

void sig_rt_handler(int signum, siginfo_t *siginfo, void *context)
{
	LOG_MSG("Succefully received RT signal from Xbar and/or GPIO expanderr and called the interrupt signal handler\n");
	syslog(LOG_INFO, "Succefull received RT signal from Xbar and/or GPIO expanderr and called the interrupt signal handler");
	
	if (get_init_flag() == 1) 
	{
		save_state();
	
		process_states();
	}
	else
	{
		LOG_MSG("Interrupt is too early, initialization is not complete\n");
		syslog(LOG_INFO, "Interrupt is too early, initialization is not complete");
	}
	
}

void cli_rt_login_handler(int signum, siginfo_t *siginfo, void *context)
{
	syslog(LOG_INFO, "Succefull received RT signal from the cli login and called cli login handler");
	LOG_MSG("Succefully received RT signal from the cli login and called cli login handler\n");
	read_shared_memory();
	
	check_login_credentials();
	send_signal_to_cli(MAIN_APP_DONE);
}

void cli_rt_handler(int signum, siginfo_t *siginfo, void *context)
{
	LOG_MSG("Succefully received RT signal from the cli and called cli handler\n");
	syslog(LOG_INFO, "Succefull received RT signal from the cli and called cli handler");
	
	process_cli_command();
	
}

void cli_pchange_handler(int signum, siginfo_t *siginfo, void *context)
{
	syslog(LOG_INFO, "Succefull received RT signal from the cli");
	LOG_MSG("Succefully received RT signal from the cli\n");
	read_shared_memory();
	
	change_p();
	send_signal_to_cli(MAIN_APP_DONE);
	
}

void send_signal_to_gui(int signal_number)
{
	LOG_MSG("Sending signal to GUI\n");
	int gui_pid = -1;
	get_gui_process_id(&gui_pid);
	LOG_MSG("GUI PID: %d\n", gui_pid);

	if(gui_pid <= 0)
	{
		LOG_ERROR(CRITICAL_ERROR, "Error: Invalid GUI PID.");
	}

	if(kill(gui_pid, signal_number) ==  -1)
	{
		LOG_ERROR(CRITICAL_ERROR, "Error: Failed to send real time signal to GUI.");
	}
}

void gui_rt_handler(int signum, siginfo_t *siginfo, void *context)
{
	LOG_MSG("Succefully received RT signal '%d' from the gui and called gui handler\n", signum);
	syslog(LOG_INFO, "Succefully received RT signal from the gui and called gui handler");

	if(signum == SIGNAL_CONFIG_CHANGE_GUI)
	{
		LOG_MSG("Configuration change signal from GUI\n");
		process_gui_command();
	}
	else if(signum == SIGNAL_HEARTBEAT_GUI)
	{
		LOG_MSG("Do nothing. Just a heartbeat check\n");
	}
	else if(signum == SIGNAL_VERIFY_GUI_LOGIN)
	{
		LOG_MSG("Check login credentials and send a signal to GUI\n");
		read_shared_memory();
		check_login_credentials();
		send_signal_to_gui(MAIN_APP_DONE);
	}
	else if(signum == SIGNAL_CHANGE_PASSWORD_GUI)
	{
		syslog(LOG_INFO, "Succefully received RT signal from the gui");
		LOG_MSG("Change password signal from GUI\n");
		read_shared_memory();
		change_p();
		send_signal_to_gui(MAIN_APP_DONE);
	}
	else if(signum == SIGNAL_SLEEP_FROM_GUI)
	{
		syslog(LOG_INFO, "Succefully received RT signal from the gui");
		LOG_MSG("Sleep signal from GUI to capture EOM data\n");

		struct timespec start, end;
		clock_gettime(CLOCK_MONOTONIC, &start);
		
		LOG_MSG("Waiting for GUI to capture EOM data\n");
		wait_for_verification_signal(EOM_CAPTURE_DONE);

		clock_gettime(CLOCK_MONOTONIC, &end);
		double time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
		LOG_MSG("Time Main app had to wait for EOM capture: %f seconds\n", time_taken);
		
		// do a reconfiguration after EOM capture to aloow any changes to take effect
		process_gui_command();
	}

	else
	{
		LOG_MSG("Do nothing.");
	}	
}

void init_interrupt_signal(void)
{
	LOG_MSG("Initializing interrupt signal\n");
	struct sigaction act;

	memset(&act, 0, sizeof(act));

	act.sa_sigaction = sig_rt_handler;
	act.sa_flags = SA_SIGINFO;
	sigemptyset(&act.sa_mask);

	if (sigaction(HW_INTTERUPT_SIGNAL, &act, NULL))
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the interrupt handler.");
		//log_error(__func__, "Failed to initialize the interrupt handler");
	}
	
}

void init_cli_signal(void)
{
	LOG_MSG("Initializing and registering signal handler for CLI\n");
	struct sigaction act;
	
	memset(&act, 0, sizeof(act));
	
	act.sa_sigaction = cli_rt_handler;
	act.sa_flags = SA_SIGINFO;
	sigemptyset(&act.sa_mask);
	
	if (sigaction(CLI_SETTING_CHANGE_SIGNAL, &act, NULL))
	{
		log_error(__func__, "Failed to initialize the cli handler");
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the cli handler.");
	}
	
}

void init_cli_pchange_signal(void)
{
	LOG_MSG("Initializing and Registering CLI password change signal handler\n");
	struct sigaction act;
	
	memset(&act, 0, sizeof(act));
	
	act.sa_sigaction = cli_pchange_handler;
	act.sa_flags = SA_SIGINFO;
	sigemptyset(&act.sa_mask);
	
	if (sigaction(CLI_PROCESS_NEW_PASSWORD, &act, NULL))
	{
		log_error(__func__, "Failed to initialize the cli handler");
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the cli handler.");
	}
	
}

void init_cli_login_signal(void)
{
	LOG_MSG("Initializing and Registering CLI login handler\n");
	struct sigaction act;
	
	memset(&act, 0, sizeof(act));
	
	act.sa_sigaction = cli_rt_login_handler;
	act.sa_flags = SA_SIGINFO;
	sigemptyset(&act.sa_mask);
	
	if (sigaction(CLI_PROCESS_LOGIN_VERIFICATION_SIGNAL, &act, NULL))
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the cli handler.");
		//log_error(__func__, "Failed to initialize the cli handler");
	}
	
}

void init_gui_signal(void)
{
	LOG_MSG("Initializing and registering signal handler for GUI\n");
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = gui_rt_handler;
	act.sa_flags = SA_SIGINFO;
	sigemptyset(&act.sa_mask);

	if (sigaction((SIGNAL_CONFIG_CHANGE_GUI), &act, NULL))
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the GUI handler for Config change.");
		//log_error(__func__, "Failed to initialize the gui handler");
	}

	if (sigaction((SIGNAL_HEARTBEAT_GUI), &act, NULL))
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the GUI handler for Heartbeat.");
		//log_error(__func__, "Failed to initialize the gui handler");
	}

	if (sigaction((SIGNAL_VERIFY_GUI_LOGIN), &act, NULL))
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the GUI handler for Login.");
		//log_error(__func__, "Failed to initialize the gui handler");
	}

	if (sigaction((SIGNAL_CHANGE_PASSWORD_GUI), &act, NULL))
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the GUI handler for Changing password.");
		//log_error(__func__, "Failed to initialize the gui handler");
	}

	if(sigaction((SIGNAL_SLEEP_FROM_GUI), &act, NULL))
	{
		LOG_ERROR(CRITICAL_ERROR, "Failed to initialize the GUI handler for Sleep signal to capture EOM data.");
		//log_error(__func__, "Failed to initialize the gui handler");
	}
	
}

void start_re_configuration(void) {
	LOG_MSG("Starting re-configuration\n");
	LOG_MSG("Running common action for specified bits.\n");

	//Call single loop configuration
	syslog(LOG_INFO, "Running common action for specified bits.\n");
    
	//single loop configuration
	configure_loop();
	
	configure_leds();
	
	syslog(LOG_INFO, "Loop Configured \r\n");
	LOG_MSG("Loop Configured\n");
}

//KM: arguement is the signal you want to wait for
void wait_for_verification_signal(int signal)
{
	LOG_MSG("Wait for verification\n");
	sigset_t set;
	struct timespec timeout;
	
	//KM: Initialize the signal settings
	sigemptyset(&set);
	sigaddset(&set, signal);
	
	//KM: Block the incoming signal, this is neccessary to use sigwait()
	if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
	{
		perror("Error: failed to block signal bedore waiting for signal");
		LOG_ERROR(CRITICAL_ERROR, "Error: failed to block signal bedore waiting for signal.");
	}
	
	//KM: set timeout time, set for 60 seconds
	timeout.tv_sec = 60;
	timeout.tv_nsec = 0;
	
	//KM: wait for the signal here (cli process will be blocked until signal is recieved or times out
	int result = sigtimedwait(&set, NULL, &timeout);

	if (result == -1) 
	{
		if (errno == EAGAIN) 
		{
			printf("Timeout occurred\n");
			LOG_ERROR(CRITICAL_ERROR, "Timed out while waiting for signal.");
		}
		else 
		{
			perror("Recieved incorrect Real Time signal");
			LOG_ERROR(CRITICAL_ERROR, "Received incorrect real time signal.");
		}
	}
	else if (result != signal)
	{
		//error
		perror("Error: recieved invalid signal");
		LOG_ERROR(CRITICAL_ERROR, "Error: recieved invalid signal.");
	}
	
	//success!
}