/* This is the main c file for a Hub application.
 Please see below for the basic architect of the code

----------------------------INDENTATION STYLE------------------------------------------------------------------------------------
All curly bracket (i.e. {}) indentations will be placed on the next line, aka Allman indetnation
examples:
void Mock_Function (void)
{
code;
};

if(condition)
{
code;
}

 --------------------------CODE STRUCTURE of C files ---------------------------------------------------------------------------------
 This is how all the delcarations and definiations should be structured in all c files
 1. #Include standard headers
 2. #Include custom headers
 3. #Define
 4. Declaration of structs & enums
 5. Declaration & initialization of global static varibales
 6. Declartion & initialization of global variables
 7. Declartion & initialization of pointers
 8. Declartion of functions
 9. Definition of functions

 ---------------------------CODE STRUCTURE of functions -----------------------------------------------------------------------------
 This is how all functions in this app will be structured
 1. Delclaration & initialization of Static variables
 2. Declaration & initialization of local variables
 3. Declaration & initailization of pointers
 3. If intialiazation of variables is not possible or inconvient during intiailzation, then intialize all local & static variables next
 4. Group lines of code with common/related tasks together and summerize actions of said group in a comment on the line before the group of code
	As a rule thumb, summarizations should not exceed more than 3 sentences, unless the complexity of the code makes it neccesary.


--------------------------------Error Handling-------------------------------------------------------------------------------------------
All errors will be immediately handled in the functions that they occur in using the custom log_error() function.
Upon encountering an error, the RFM Hub will also exit the program with exit code 1 i.e. exit(1) (this will happen inside the log_error() funciton)
Example.
if(error_condition==true)
{
	log_error(__func__, "Description of failed operation");
}

 -----------------------------NAMING CONVENTION ----------------------------------------------------------------------------------
 The code for this application will adhere to the following naming conventions for custom variables and functions
 Functions that do not follow this naming convention are standard functions to the Linux OS

 Defines - ALLCAPS

 Gloable variables - Capatilized (aka pascal case; LooksLikeThis)

 Local variables - Camel case (looksLikeThis)

 Functions - snake Case (looks_like_this) (Note: acronums will still be capatalized)

 !!Note the refactoring to abide by these rules is still in progress
 */



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>

#include "GPIOExpander.h"
#include "LoopSetup.h"
#include "XBar.h"
#include "LED.h"
#include "I2CCommon.h"
#include "SharedMemory.h"
#include "util.h"
#include "interrupts.h"
#include "crypto.h"
#include "ChannelStatus.h"

static bool initflag = 0;

bool get_init_flag(void)
{
	LOG_MSG("Get the init flag\n");
	return initflag;
}

static void init(void)
{
	LOG_MSG("Initializing main RFMHub application\n");
	//KM: let the user know that the Hub is powered on and intializing 
	power_LED_on();
	
	//KM: to avoid using the I2C bus during heavy traffic at boot, wait 15 seconds
	sleep(15);
	
	init_interrupt_signal();
	
	init_cli_login_signal();
	
	init_cli_signal();

	init_gui_signal();
	
	init_cli_pchange_signal();
	
	LEDs_off(); //Turns all LEDs off until init is complete
	
	open_I2C_bus();
	
	init_GPIO_expanders();
	
	init_xbar();
	
	init_shared_memory();
	
	init_share_public_key();
	
	init_read_config();
	
	init_update_channel_array();
	
	configure_loop();
	
	init_LEDs();
	
	//KM: flag that initilazation is done
	initflag = 1;
	
	//KM if not running in debug mode, send signal to gui
	#ifndef DEBUG
		send_signal_to_gui(MAIN_APP_INITIALIZED);                
	#endif // !DEBUG
}

int main() 
{

	LOG_MSG("Main application starting\n");
    init();
	
	LOG_MSG("Initialization done!");
	
    while (1) {        
	    LOG_MSG("RFMHub running\n");
	    syslog(LOG_INFO, "RFM Hub running");
        sleep(1); // Sleep for 1 seconds
	    
	    
	    //KM: The Xbar interrupt line is no longer connected to the SOM ever since we switched from SOM6 to SOM8
	    // Therefore we need to poll the CDR now instead of relying on interrupts.
	    disable_interrupt_repsonses();
	    
	    poll_CDR();
	    update_CDR_status();
	    configure_leds();
	    
	    enable_interrupt_repsonses();
    }
	
	close_I2C_bus();
	close_shared_memory();
	
    // Close the log
    closelog();
	
    return EXIT_SUCCESS;
}
