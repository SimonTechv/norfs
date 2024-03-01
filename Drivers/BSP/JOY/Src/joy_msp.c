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
#include "button.h"

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

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/
static void JoyLongRelease(uint8_t btnCode);
static void JoyLongPress(uint8_t btnCode);
static void JoyShortRelease(uint8_t btnCode);


/************************************
 * GLOBAL VARIABLES
 ************************************/
TIM_HandleTypeDef htim1;

/* Joytick keys define port&pins */
btn_instance_t jst_keys[5] =
{
        {
                .port = JOY_DOWN_GPIO_Port,
                .pin  = JOY_DOWN_Pin,
        },
        {
                .port = JOY_UP_GPIO_Port,
                .pin  = JOY_UP_Pin
        },
        {
                .port = JOY_LEFT_GPIO_Port,
                .pin  = JOY_LEFT_Pin
        },
        {
                .port = JOY_RIGHT_GPIO_Port,
                .pin  = JOY_RIGHT_Pin
        },
        {
                .port = JOY_SEL_GPIO_Port,
                .pin  = JOY_SEL_Pin
        }
};

/* Press button module init parameters */
btn_init_t jst_params =
{
        /* Parameters button processing */
        .instance          = &jst_keys[0],
        .debounce_time_ms  = DEBOUNCE_TIME,
        .long_press_def_ms = LONG_PRESS_MS,
        .process_time_ms   = PROC_TIME_PERIOD,

        /* Processing callbacks */
        .port_read     = &HAL_GPIO_ReadPin,
        .short_release = &JoyShortRelease,
        .long_press    = &JoyLongPress,
        .long_release  = &JoyLongRelease
};


/* KBRD CONTROL STRUCT */
__IO joystick joy = {0,0,0,0,0};


/************************************
 * STATIC FUNCTIONS
 ************************************/

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
    if (Button_Init(&jst_params, jst_keys, JOY_BTN_CNT) != 0)
    {
        return -1;
    }

    /* Start keboard polling */
    if(HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)
    {
        return -1;
    }

    /* All operations done */
    return ret;

}


/******************************************************************************/
/*                         Button processing callbacks                        */
/******************************************************************************/

/**
 * @brief
 *
 * @param btnCode
 */
static void JoyShortRelease(uint8_t btnCode)
{
    switch (btnCode)
    {
    case 0:
        joy.down   = SET;
        break;
    case 1:
        joy.up     = SET;
        break;
    case 2:
        joy.left   = SET;
        break;
    case 3:
        joy.right  = SET;
        break;
    case 4:
        joy.sel    = SET;
        break;
    };
}

/**
 * @brief
 *
 * @param btnCode
 */
static void JoyLongPress(uint8_t btnCode)
{
    __NOP();
}

/**
 * @brief
 *
 * @param btnCode
 */
static void JoyLongRelease(uint8_t btnCode)
{
    __NOP();
}


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
    Button_Update();
}

