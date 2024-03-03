/**
 ********************************************************************************
 * @file    joy_msp.c
 * @author  SimON
 * @date    Mar 1, 2024
 * @brief   
 ********************************************************************************
 */

/************************************
 * INCLUDES
 ************************************/
#include "joy_msp.h"
#include "stm32f4xx.h"
#include "gpio_defs.h"
#include "multi_button.h"

/************************************
 * EXTERN VARIABLES
 ************************************/

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

/************************************
 * PRIVATE TYPEDEFS
 ************************************/

/************************************
 * STATIC VARIABLES
 ************************************/

enum joy_keys {
    JOY_UP,
    JOY_DOWN,
    JOY_LEFT,
    JOY_RIGHT,
    JOY_SEL
};


/* Button handles */
static Button btn_up;
static Button btn_down;
static Button btn_left;
static Button btn_right;
static Button btn_select;


/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/

static void JOY_StructureInit(void);
static uint8_t JOY_PollGPIO(uint8_t);

/************************************
 * GLOBAL VARIABLES
 ************************************/
TIM_HandleTypeDef htim1;

/* KBRD CONTROL STRUCT */
__IO joystick joy = {0,0,0,0,0};


/************************************
 * GLOBAL FUNCTIONS
 ************************************/

/**
 * @brief Initialize button processing module
 *
 * @return -1 if error occured
 */
int8_t JOY_ProcessingInit(void)
{
    int8_t ret = 0;

    /* TIM1 peripheral clock enable */
    __HAL_RCC_TIM1_CLK_ENABLE();

    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    /* USER CODE BEGIN TIM1_Init 1 */

    /* USER CODE END TIM1_Init 1 */
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 50000 - 1;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 20 - 1;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
        return -1;
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
    {
        return -1;
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        return -1;
    }

    /* TIM1 interrupt setup */
    HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);

    /* Initialize button module */
    JOY_StructureInit();

    /* Start keboard polling */
    if(HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)
    {
        return -1;
    }

    /* All operations done */
    return ret;

}


/************************************
 * STATIC FUNCTIONS
 ************************************/

/**
 * @brief Polling gpio level
 *
 */
static uint8_t JOY_PollGPIO(uint8_t)
{

}

/**
 * @brief Initialize button processing module
 *
 */
static void JOY_StructureInit(void)
{
    /* Configure button UP */
    button_init(&btn_up, &JOY_PollGPIO, GPIO_PIN_SET, JOY_UP);
    button_init(&btn_down, &JOY_PollGPIO, GPIO_PIN_SET, JOY_DOWN);
    button_init(&btn_down, &JOY_PollGPIO, GPIO_PIN_SET, JOY_DOWN);


}


/******************************************************************************/
/*                         Button processing callbacks                        */
/******************************************************************************/




/******************************************************************************/
/*                         Button processing timer IRQ HANDLER                */
/******************************************************************************/
/**
 * @brief Button processing routine
 *
 * calling every
 *
 */
void JOY_ProcessingRoutine(void)
{
    /* Perform button polling */
    button_ticks();
}

