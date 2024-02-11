/*
 * nor_driver.c
 *
 *  Created on: 24 янв. 2024 г.
 *      Author: SimON
 */
#include "nor_driver.h"
#include "lx_api.h"
#include "stm32f4xx.h"
#include "gpio_defs.h"

// QSPI дескриптор
QSPI_HandleTypeDef QSPIHandle;

// Флаги завершения чтения/записи данных через DMA
UCHAR __IO TxCplt, RxCplt = 0;

// ULONG size aligned buffer for driver sector access
__attribute__((aligned(4))) uint8_t sector_buffer[DRIVER_LOG_SECTOR_SIZE] = {0};

__attribute__((aligned(4))) uint8_t verify_sector_buffer[DRIVER_LOG_SECTOR_SIZE] = {0};


/* Local fuctions prototypes */
//UINT                            (*lx_nor_flash_driver_read)(ULONG *flash_address, ULONG *destination, ULONG words);
//UINT                            (*lx_nor_flash_driver_write)(ULONG *flash_address, ULONG *source, ULONG words);
//UINT                            (*lx_nor_flash_driver_block_erase)(ULONG block, ULONG erase_count);
//UINT                            (*lx_nor_flash_driver_block_erased_verify)(ULONG block);
//UINT                            (*lx_nor_flash_driver_system_error)(UINT error_code);

/* Driver auxiliary functions*/
UINT _driver_qspi_init();
UINT _driver_nor_flash_wait_eop();
UINT _driver_nor_flash_write_enable();
UINT _driver_nor_flash_configure();
UINT _driver_nor_flash_page_prog(ULONG address, UCHAR* data, ULONG size);


/* IO driver main functions */
UINT _driver_nor_flash_block_erase(ULONG block, ULONG erase_count);
UINT _driver_nor_flash_read(ULONG *flash_address, ULONG *destination, ULONG words);
UINT _driver_nor_flash_write(ULONG *flash_address, ULONG *source, ULONG words);
UINT _driver_nor_flash_erased_verify(ULONG block);
UINT _driver_nor_flash_system_error(UINT error_code);



/* Global driver functions */

/**
 * Initialize NOR flash memory driver
 * @param instance Driver settings instance
 * @return
 */
UINT flash_driver_init(LX_NOR_FLASH *instance)
{
    // Setting up driver params
    instance->lx_nor_flash_base_address     = (ULONG *)DRIVER_BASE_OFFSET_MEM;
    instance->lx_nor_flash_total_blocks     = DRIVER_BLOCK_COUNT;
    instance->lx_nor_flash_words_per_block  = DRIVER_WORDS_PER_BLOCK;

    // RAM buffer 512 bytes aligned by 4 bytes
    instance->lx_nor_flash_sector_buffer   = (ULONG*)&sector_buffer[0];

    // Link driver function
    instance->lx_nor_flash_driver_read                  = _driver_nor_flash_read;
    instance->lx_nor_flash_driver_write                 = _driver_nor_flash_write;
    instance->lx_nor_flash_driver_block_erase           = _driver_nor_flash_block_erase;
    instance->lx_nor_flash_driver_system_error          = _driver_nor_flash_system_error;
    instance->lx_nor_flash_driver_block_erased_verify   = _driver_nor_flash_erased_verify;

    // Initialize phy interface
    if (_driver_qspi_init() != LX_SUCCESS)
    {
        return LX_ERROR;
    }

    // Configure flash memory
    if (_driver_nor_flash_configure() != LX_SUCCESS)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
}

/**
 * @brief Perform read in QSPI mode
 *
 * @param flash_address
 * @param destination
 * @param words (Count of 4-byte words)
 * @return status of operation
 */
UINT _driver_nor_flash_read(ULONG *flash_address, ULONG *destination, ULONG words)
{

    // Is address valid ?
    if (flash_address > DRIVER_HIGHER_ADDRESS_FLASH_MEMORY || flash_address < DRIVER_LOWER_ADDRESS_FLASH_MEMORY)
    {
        // IO error !
        _driver_nor_flash_system_error(LX_ERROR);
    }

    QSPI_CommandTypeDef cmd;

    /* Command params struct fill */
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AddressMode       = QSPI_ADDRESS_4_LINES;
    cmd.DataMode          = QSPI_DATA_4_LINES;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    /* Read data */
    cmd.Instruction = QUAD_INOUT_FAST_READ_CMD;
    cmd.Address     = (ULONG)flash_address;
    cmd.DummyCycles = N25Q128A_VCR_NB_DUMMY >> 4;
    cmd.NbData = words * sizeof(ULONG); // Data transfer size in bytes

    if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    if (HAL_QSPI_Receive_DMA(&QSPIHandle, (uint8_t*)destination) != HAL_OK)
    {
        return LX_ERROR;
    }

    // Waiting EOR
    while(RxCplt == 0);
    RxCplt = 0;

    return LX_SUCCESS;
}

/**
 * @brief Perform error processing for LevelX API
 *
 * @param error_code
 * @return status of operation
 */
UINT _driver_nor_flash_system_error(UINT error_code)
{
    (void) error_code;
    __NOP();

    return LX_SUCCESS;
}

/**
 * @brief Write 4-byte aligned data
 *
 * @param address : Address in FLASH memory
 * @param source  : Pointer to data buffer
 * @param words   : Buffer size
 * @return        : Status of operation
 */
UINT _driver_nor_flash_write(ULONG *flash_address, ULONG *source, ULONG words)
{

    // Is address valid ?
    if (flash_address > DRIVER_HIGHER_ADDRESS_FLASH_MEMORY || flash_address < DRIVER_LOWER_ADDRESS_FLASH_MEMORY)
    {
        // IO error !
        _driver_nor_flash_system_error(LX_ERROR);
    }

    ULONG words_per_prog_page = (N25_PAGE_PROG_SIZE / sizeof(ULONG));

    // Program by 256 byte page (in terms of levelx : 64 (4-byte) WORD)
    while (words >= words_per_prog_page)
    {
        if (_driver_nor_flash_page_prog((ULONG)flash_address, (UCHAR *)source, N25_PAGE_PROG_SIZE) != LX_SUCCESS)
        {
            return LX_ERROR;
        }

        // Increase address and source buffer pointer
        flash_address  += N25_PAGE_PROG_SIZE;
        source    += words_per_prog_page;
        words     -= words_per_prog_page;
    }

    // If data size unaligned by page prog size (write residue bytes)
    if (words != 0)
    {
        if (_driver_nor_flash_page_prog((ULONG)flash_address,  (UCHAR *)source, words * sizeof(ULONG)) != LX_SUCCESS)
        {
            return LX_ERROR;
        }
    }

    return LX_SUCCESS;
}



/**
 * @brief Place 256-byte page into selected area by address
 *
 * @param address: Address in FLASH memory
 * @param data:    Pointer to data buffer
 * @param size:    Buffer size (in bytes)
 * @return
 */
UINT _driver_nor_flash_page_prog(ULONG address, UCHAR* data, ULONG size)
{
    QSPI_CommandTypeDef cmd;

    if (_driver_nor_flash_write_enable() != LX_SUCCESS)
    {
        return LX_ERROR;
    }

    /* Command params struct fill */
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode       = QSPI_ADDRESS_4_LINES;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode          = QSPI_DATA_4_LINES;
    cmd.DummyCycles       = 0;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    /* Write data command*/
    cmd.Instruction = EXT_QUAD_IN_FAST_PROG_CMD;
    cmd.Address     = address;
    cmd.DummyCycles = 0;
    cmd.NbData      = size; // Data transfer size in bytes

    if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    if (HAL_QSPI_Transmit_DMA(&QSPIHandle, (uint8_t*)data) != HAL_OK)
    {
        return LX_ERROR;
    }

    // Waiting EOT
    while(TxCplt == 0);
    TxCplt = 0;

    // Wait EOP
    if (_driver_nor_flash_wait_eop() != LX_SUCCESS)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
}

/**
 * @brief Perform SUBSECTOR erase (4-kByte size sector)
 *
 * @param block
 * @param erase_count
 * @return
 */
UINT _driver_nor_flash_block_erase(ULONG block, ULONG erase_count)
{

    // Is block valid ?
    if (block > DRIVER_HIGH_BLK_IDX || block < DRIVER_LOW_BLK_IDX)
    {
        // IO error !
        _driver_nor_flash_system_error(LX_ERROR);
    }

    UINT ret = LX_SUCCESS;
    QSPI_CommandTypeDef cmd;

    // Write enable latch set
    if (_driver_nor_flash_write_enable() != LX_SUCCESS)
    {
        return LX_ERROR; // Exit if error occured
    }

    /* Erasing Sequence -------------------------------------------------- */
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    cmd.Instruction = SUBSECTOR_ERASE_CMD;
    cmd.AddressMode = QSPI_ADDRESS_1_LINE;
    cmd.DataMode    = QSPI_DATA_NONE;
    cmd.DummyCycles = 0;

    for (ULONG blk = 0; blk < erase_count; blk++)
    {
        // Calculate block address
        cmd.Address = (ULONG)DRIVER_BASE_OFFSET_MEM + (block + blk) * DRIVER_PHY_BLOCK_SIZE;
        // Send ERASE command
        if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
        {
            ret = LX_ERROR;
            break;
        }

        // Wait end of end ERASE sector sequence
        if (_driver_nor_flash_wait_eop() != LX_SUCCESS)
        {
            ret = LX_ERROR;
            break;
        }
    }

    return ret;
}

/**
 * Perform QUADSPI MCU peripheral init + setting up memory
 * @return Status of initialization process
 */
UINT _driver_qspi_init()
{
    /* Initialize QuadSPI ------------------------------------------------------ */
    QSPIHandle.Instance = QUADSPI;
    HAL_QSPI_DeInit(&QSPIHandle);

    QSPIHandle.Init.ClockPrescaler = 0;
    QSPIHandle.Init.FifoThreshold = 4;
    QSPIHandle.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    QSPIHandle.Init.FlashSize = QSPI_FLASH_SIZE;
    QSPIHandle.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_6_CYCLE;
    QSPIHandle.Init.ClockMode = QSPI_CLOCK_MODE_0;
    QSPIHandle.Init.FlashID = QSPI_FLASH_ID_1;
    QSPIHandle.Init.DualFlash = QSPI_DUALFLASH_DISABLE;

    if(HAL_QSPI_Init(&QSPIHandle) != HAL_OK)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
}

/**
 * @brief Configure enchanced volatile configuration register
 * # Setting up DUMMYCLK for perform read operations
 *
 * @return Error code
 */
UINT _driver_nor_flash_configure()
{
    QSPI_CommandTypeDef cmd;

    // Set WEL
    if (_driver_nor_flash_write_enable() != LX_SUCCESS)
    {
       return LX_ERROR;
    }

    // Set dummy clock count
    uint8_t vcr_reg_w = 0x0F | N25Q128A_VCR_NB_DUMMY;
    uint8_t vcr_reg_r = 0;

    cmd.Instruction         = WRITE_VOL_CFG_REG_CMD;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode         = QSPI_ADDRESS_NONE;
    cmd.DataMode            = QSPI_DATA_1_LINE;
    cmd.AddressSize         = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
    cmd.AlternateBytesSize  = 0;
    cmd.DummyCycles         = 0;
    cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
    cmd.NbData              = 1;

    if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    if (HAL_QSPI_Transmit(&QSPIHandle, &vcr_reg_w, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
       return LX_ERROR;
    }

    /* Check dummy clock count */
    cmd.Instruction       = READ_VOL_CFG_REG_CMD;

    if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    if (HAL_QSPI_Receive(&QSPIHandle, &vcr_reg_r, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    if ((vcr_reg_r & N25Q128A_VCR_NB_DUMMY) != N25Q128A_VCR_NB_DUMMY)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
}


/**
 * @brief This function read the SR of the memory and wait the EOP.
 *
 * @return Error code
 */
UINT _driver_nor_flash_wait_eop()
{
    QSPI_CommandTypeDef cmd;
    QSPI_AutoPollingTypeDef cfg;

    /* Configure automatic polling mode to wait for memory ready ------ */
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction = READ_STATUS_REG_CMD;
    cmd.AddressMode = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode = QSPI_DATA_1_LINE;
    cmd.DummyCycles = 0;
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    cfg.Match = 0x00;
    cfg.Mask = 0x01;
    cfg.MatchMode = QSPI_MATCH_MODE_AND;
    cfg.StatusBytesSize = 1;
    cfg.Interval = 0x10;
    cfg.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling(&QSPIHandle, &cmd, &cfg, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
}


/**
 * @brief This function send a Write Enable and wait in extended SPI mode
 *
 * @return Error code
 */
UINT _driver_nor_flash_write_enable()
{
    QSPI_CommandTypeDef cmd;
    QSPI_AutoPollingTypeDef cfg;

    /* Enable write operations ------------------------------------------ */
    cmd.Instruction        = WRITE_ENABLE_CMD;
    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.DataMode           = QSPI_DATA_NONE;
    cmd.AlternateByteMode  = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles        = 0;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    /* Configure automatic polling mode to wait for write enabling ---- */
    cmd.Instruction     = READ_STATUS_REG_CMD;
    cmd.DataMode        = QSPI_DATA_1_LINE;
    cfg.Match           = 0x02;
    cfg.Mask            = 0x02;
    cfg.MatchMode       = QSPI_MATCH_MODE_AND;
    cfg.StatusBytesSize = 1;
    cfg.Interval        = 0x1;
    cfg.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling(&QSPIHandle, &cmd, &cfg, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;

}


/**
 * @brief Perform test read/write driver functionality
 *
 */
void _driver_test_write()
{
}

/**
 * @brief Verify block erase
 *
 * @param block : Number of block (SUBSECTOR for this flash memory)
 * @return block erase status
 */
UINT _driver_nor_flash_erased_verify(ULONG block)
{
    ULONG* block_start_addr = DRIVER_BASE_OFFSET_MEM + block * DRIVER_PHY_BLOCK_SIZE;
    ULONG*  block_stop_addr = block_start_addr + DRIVER_PHY_BLOCK_SIZE;

    // Partially verify erased block
    for (; block_start_addr < block_stop_addr; block_start_addr += DRIVER_LOG_SECTOR_SIZE)
    {
        // Read 512 byte part of block
        if (_driver_nor_flash_read(block_start_addr, (ULONG *)verify_sector_buffer, DRIVER_WORDS_PER_SECTOR) != LX_SUCCESS)
        {
            return LX_ERROR;
        }

        // Verify erased block
        for (ULONG i = 0; i < DRIVER_LOG_SECTOR_SIZE; i++)
        {
            if (verify_sector_buffer[i] != 0xFF)
            {
                // Sector erased failed!
                 return LX_ERROR;
            }
        }
    }

    return LX_SUCCESS;
}
//void HAL_QSPI_CmdCpltCallback      (QSPI_HandleTypeDef *hqspi)

void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
   RxCplt = 1;
}

void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    TxCplt = 1;
}



