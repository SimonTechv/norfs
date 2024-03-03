/**
 ********************************************************************************
 * @file    joy_msp.h
 * @author  SimON
 * @date    Mar 1, 2024
 * @brief   
 ********************************************************************************
 */

#ifndef BSP_JOY_JOY_MSP_H_
#define BSP_JOY_JOY_MSP_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include "stdint.h"
#include "stm32f4xx.h"

/************************************
 * MACROS AND DEFINES
 ************************************/
#define JOY_BTN_CNT         5
#define LONG_PRESS_MS       3000
#define PROC_TIME_PERIOD    10
#define DEBOUNCE_TIME       20

/************************************
 * TYPEDEFS
 ************************************/

typedef struct
{
    uint8_t up;
    uint8_t down;
    uint8_t right;
    uint8_t left;
    uint8_t sel;
} joystick;


/************************************
 * EXPORTED VARIABLES
 ************************************/

extern TIM_HandleTypeDef htim1;
extern __IO joystick joy;


/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
int8_t JOY_ProcessingInit(void);
void JOY_ProcessingRoutine(void);


#ifdef __cplusplus
}
#endif

#endif 
