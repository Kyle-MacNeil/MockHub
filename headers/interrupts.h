#pragma once

#include <signal.h>
#include "interrupts.h"

#define HW_INTTERUPT_SIGNAL SIGRTMIN
#define CLI_SETTING_CHANGE_SIGNAL (SIGRTMIN+1)
#define CLI_PROCESS_LOGIN_VERIFICATION_SIGNAL (SIGRTMIN +2)
#define MAIN_APP_DONE (SIGRTMIN +3)
#define CLI_PROCESS_NEW_PASSWORD (SIGRTMIN +4)
// GUI real time signals
#define SIGNAL_CONFIG_CHANGE_GUI SIGRTMIN+5
#define SIGNAL_HEARTBEAT_GUI SIGRTMIN+6
#define SIGNAL_VERIFY_GUI_LOGIN SIGRTMIN+7
#define SIGNAL_CHANGE_PASSWORD_GUI SIGRTMIN+8
#define SIGNAL_DEFAULT_GUI_SETTINGS  SIGRTMIN+9
#define GUI_DONE SIGRTMIN+10
#define SIGNAL_SLEEP_FROM_GUI SIGRTMIN+11
#define EOM_CAPTURE_DONE SIGRTMIN+12
#define MAIN_APP_INITIALIZED SIGRTMIN + 13

void init_cli_signal(void);
void init_gui_signal(void);
void init_interrupt_signal(void);
void start_re_configuration(void);
void init_cli_login_signal(void);
void cli_pchange_handler(int signum, siginfo_t *siginfo, void *context);
void init_cli_pchange_signal(void);
void send_signal_to_gui(int signal_number);
void wait_for_verification_signal(int signal);
void CDR_poll_response(void);
void enable_interrupt_repsonses(void);
void disable_interrupt_repsonses(void);
