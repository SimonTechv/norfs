/*
 * nor_driver.h
 *
 *  Created on: 24 янв. 2024 г.
 *      Author: SimON
 *      Brief: This driver implements necessary driver functions for working with levelx driver
 *      For devboard stm32f412 discovery QSPI flash MICRON N25Q128A13EF840E
 *
 *      Technical characteristics:
 *      1. Memory capacity: 128 Mbit -> 16 MByte
 *      2. Minimal erase size: 4Kbyte (subblock size)
 *      3. 4K sectors count 4096
 *
 */

#ifndef INC_NOR_DRIVER_H_
#define INC_NOR_DRIVER_H_

#include "lx_api.h"

/* N25Q128A13EF840E Micron memory params*/
#define QSPI_FLASH_SIZE                      23
#define QSPI_PAGE_SIZE                       256
#define N25_SUBSECTOR_SIZE                   4096
#define N25_SUBSECTOR_COUNT                  4096
#define N25_SECTOR_SIZE                      65536
#define N25_SECTOR_COUNT                     256
#define N25_MEMORY_SIZE                      N25_SUBSECTOR_COUNT * N25_SUBSECTOR_SIZE
#define N25_BASE_ADDR                        0x000000
#define N25_HIGH_ADDR                        0xFFFFFF
#define N25_LOW_SS_IDX                       0
#define N25_HIGH_SS_IDX                      4095
#define N25_PAGE_PROG_SIZE                   256



/* Driver block and sector size matched with phy flash memory block and size*/
#define DRIVER_BLOCK_SIZE                    N25_SECTOR_SIZE
#define DRIVER_BLOCK_COUNT                   N25_SECTOR_COUNT - 1
#define DRIVER_BASE_OFFSET_MEM               0x90000000
#define DRIVER_LOWER_ADDRESS_FLASH_MEMORY    DRIVER_BASE_OFFSET_MEM + N25_BASE_ADDR
#define DRIVER_HIGHER_ADDRESS_FLASH_MEMORY   DRIVER_BASE_OFFSET_MEM + N25_HIGH_ADDR
#define DRIVER_LOW_BLK_IDX                   N25_LOW_SS_IDX
#define DRIVER_HIGH_BLK_IDX                  N25_HIGH_SS_IDX


/* Default timeout (ms) */
#define N25Q128A_BULK_ERASE_MAX_TIME         480000
#define N25Q128A_SECTOR_ERASE_MAX_TIME       3000
#define N25Q128A_SUBSECTOR_ERASE_MAX_TIME    8000

#define N25Q128A_DEFAULT_TIMEOUT             1000

/* Reset Operations */
#define RESET_ENABLE_CMD                     0x66
#define RESET_MEMORY_CMD                     0x99

/* Identification Operations */
#define READ_ID_CMD                          0x9E
#define READ_ID_CMD2                         0x9F
#define MULTIPLE_IO_READ_ID_CMD              0xAF
#define READ_SERIAL_FLASH_DISCO_PARAM_CMD    0x5A

/* Read Operations */
#define READ_CMD                             0x03
#define FAST_READ_CMD                        0x0B
#define DUAL_OUT_FAST_READ_CMD               0x3B
#define DUAL_INOUT_FAST_READ_CMD             0xBB
#define QUAD_OUT_FAST_READ_CMD               0x6B
#define QUAD_INOUT_FAST_READ_CMD             0xEB

/* Write Operations */
#define WRITE_ENABLE_CMD                     0x06
#define WRITE_DISABLE_CMD                    0x04

/* Register Operations */
#define READ_STATUS_REG_CMD                  0x05
#define WRITE_STATUS_REG_CMD                 0x01

#define READ_LOCK_REG_CMD                    0xE8
#define WRITE_LOCK_REG_CMD                   0xE5

#define READ_FLAG_STATUS_REG_CMD             0x70
#define CLEAR_FLAG_STATUS_REG_CMD            0x50

#define READ_NONVOL_CFG_REG_CMD              0xB5
#define WRITE_NONVOL_CFG_REG_CMD             0xB1

#define READ_VOL_CFG_REG_CMD                 0x85
#define WRITE_VOL_CFG_REG_CMD                0x81

#define READ_ENHANCED_VOL_CFG_REG_CMD        0x65
#define WRITE_ENHANCED_VOL_CFG_REG_CMD       0x61

/* Program Operations */
#define PAGE_PROG_CMD                        0x02
#define DUAL_IN_FAST_PROG_CMD                0xA2
#define EXT_DUAL_IN_FAST_PROG_CMD            0xD2
#define QUAD_IN_FAST_PROG_CMD                0x32
#define EXT_QUAD_IN_FAST_PROG_CMD            0x12

/* Erase Operations */
#define SUBSECTOR_ERASE_CMD                  0x20
#define SECTOR_ERASE_CMD                     0xD8
#define BULK_ERASE_CMD                       0xC7
#define PROG_ERASE_RESUME_CMD                0x7A
#define PROG_ERASE_SUSPEND_CMD               0x75

/* One-Time Programmable Operations */
#define READ_OTP_ARRAY_CMD                   0x4B
#define PROG_OTP_ARRAY_CMD                   0x42

/* Volatile Configuration Register */
#define N25Q128A_VCR_WRAP                    ((uint8_t)0x03)    /*!< Wrap */
#define N25Q128A_VCR_XIP                     ((uint8_t)0x08)    /*!< XIP */
#define N25Q128A_VCR_NB_DUMMY                ((uint8_t)0xA0)    /*!< Number of dummy clock cycles */

/* Flag Status Register */
#define N25Q128A_FSR_PRERR                   ((uint8_t)0x02)    /*!< Protection error */
#define N25Q128A_FSR_PGSUS                   ((uint8_t)0x04)    /*!< Program operation suspended */
#define N25Q128A_FSR_VPPERR                  ((uint8_t)0x08)    /*!< Invalid voltage during program or erase */
#define N25Q128A_FSR_PGERR                   ((uint8_t)0x10)    /*!< Program error */
#define N25Q128A_FSR_ERERR                   ((uint8_t)0x20)    /*!< Erase error */
#define N25Q128A_FSR_ERSUS                   ((uint8_t)0x40)    /*!< Erase operation suspended */
#define N25Q128A_FSR_READY                   ((uint8_t)0x80)    /*!< Ready or command in progress */


/**
 * Exported driver function prototypes
 */
UINT flash_driver_init(LX_NOR_FLASH *instance);
UINT flash_driver_deinit(void);
UINT _driver_nor_flash_bulk_erase(void);

#endif /* INC_NOR_DRIVER_H_ */
