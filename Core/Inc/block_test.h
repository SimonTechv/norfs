/**
 ********************************************************************************
 * @file    block_test.h
 * @author  SimON
 * @date    21 февр. 2024 г.
 * @brief   
 ********************************************************************************
 */

#ifndef INC_BLOCK_TEST_H_
#define INC_BLOCK_TEST_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include "gpio_defs.h"
#include "stm32f4xx.h"

/************************************
 * MACROS AND DEFINES
 ************************************/

/************************************
 * TYPEDEFS
 ************************************/
typedef enum
{
    PASSED,
    ERASE_FAILED,
    ERASE_VERIFY_FAILED,
    WRITING_FAILED,
    READING_FAILED,
    COMPARE_FAILED
} test_status;

/************************************
 * EXPORTED VARIABLES
 ************************************/
extern __attribute__((aligned(4))) const uint8_t data_pattern[512];
/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/


#ifdef __cplusplus
}
#endif

#endif 
