#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "logging.h"
/*
 * GPIOExpander.h
 **/

#define SetAsInputPin  1
#define SetAsOutputPin 0

typedef struct
{
    uint16_t RX_LOS0    : 1;
    uint16_t RX_LOS1    : 1;
    uint16_t RX_LOS2    : 1;
    uint16_t RX_LOS3    : 1;
    uint16_t RX_LOS4    : 1;
    uint16_t RX_LOS5    : 1;
    uint16_t RX_LOS6    : 1;
    uint16_t RX_LOS7    : 1;
    uint16_t RX_LOS8    : 1;
    uint16_t RX_LOS9    : 1;
    
    uint16_t reserved_0 : 1;
    uint16_t reserved_1 : 1;
    uint16_t reserved_2 : 1;
    uint16_t reserved_3 : 1;
    
    uint16_t Soft_Reset_l   : 1;
    uint16_t S_D_Sel        : 1;
		
}BitsRXLOSS;

typedef struct
{
    uint16_t TX_Fault_0 : 1;
    uint16_t TX_Fault_1 : 1;
    uint16_t TX_Fault_2 : 1;
    uint16_t TX_Fault_3 : 1;
    uint16_t TX_Fault_4 : 1;
    uint16_t TX_Fault_5 : 1;
    uint16_t TX_Fault_6 : 1;
    uint16_t TX_Fault_7 : 1;
    uint16_t TX_Fault_8 : 1;
    uint16_t TX_Fault_9 : 1;
    
    uint16_t reserved_0 : 1;
    uint16_t reserved_1 : 1;
    uint16_t reserved_2 : 1;
    uint16_t reserved_3 : 1;
    uint16_t reserved_4 : 1;
    uint16_t reserved_5 : 1;

}BitsTXFault;

typedef struct 
{
    uint16_t TX_Disable0 : 1;
    uint16_t TX_Disable1 : 1;
    uint16_t TX_Disable2 : 1;
    uint16_t TX_Disable3 : 1;
    uint16_t TX_Disable4 : 1;
    uint16_t TX_Disable5 : 1;
    uint16_t TX_Disable6 : 1;
    uint16_t TX_Disable7 : 1;
    uint16_t TX_Disable8 : 1;
    uint16_t TX_Disable9 : 1;

    uint16_t reserved_0 : 1;
    uint16_t reserved_1 : 1;
    
    uint16_t  EEProm_wr_l: 1;
    uint16_t  MUX_RST: 1;
    uint16_t  TR_RS0: 1;
    uint16_t  TR_RS1: 1;    
}TXDisableBits;

typedef struct 
{
    uint16_t  MOD_ABS0 : 1;
    uint16_t  MOD_ABS1 : 1;
    uint16_t  MOD_ABS2 : 1;
    uint16_t  MOD_ABS3 : 1;
    uint16_t  MOD_ABS4 : 1;
    uint16_t  MOD_ABS5 : 1;
    uint16_t  MOD_ABS6 : 1;
    uint16_t  MOD_ABS7 : 1;
    uint16_t  MOD_ABS8 : 1;
    uint16_t  MOD_ABS9 : 1;
    
    uint16_t reserved_0 : 1;
    uint16_t reserved_1 : 1;
    uint16_t reserved_2 : 1;
    uint16_t reserved_3 : 1;
    uint16_t reserved_4 : 1;
    uint16_t reserved_5 : 1;
}ModuleAbsentBits;

// Data structure to save the state of the the GPIO Expander input register values

typedef struct 
{
    BitsRXLOSS  rx_loss;
    BitsTXFault tx_fault;
    TXDisableBits tx_disable;
    ModuleAbsentBits mod_absent;

}ExpanderStates;


void configure_GPIO_expander(void);
void clear_IO_expanders_interrupt(void);
void process_states(void);
ExpanderStates* init_get_GPIO_expander_state(void);
uint16_t get_SFP_SD(void);
void init_GPIO_expanders(void);
ExpanderStates* save_state_GPIO_expander(void);
uint64_t compare_GPIO_states(void);
void assert_TX_disable(uint16_t TXdisable);
void change_hub_mode(void);
uint16_t get_reset_pin_state(void);

// **end**
