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

#include "block_test.h"

// QSPI дескриптор
QSPI_HandleTypeDef QSPIHandle;

// Флаги завершения чтения/записи данных через DMA
UCHAR __IO TxCplt, RxCplt, CmdCplt = 0;

// ULONG size aligned buffer for driver sector access
__attribute__((aligned(4))) ULONG sector_buffer[LX_NOR_SECTOR_SIZE] = {0};

__attribute__((aligned(4))) ULONG verify_sector_buffer[LX_NOR_SECTOR_SIZE] = {0};

/* Local fuctions prototypes */
//UINT                            (*lx_nor_flash_driver_read)(ULONG *flash_address, ULONG *destination, ULONG words);
//UINT                            (*lx_nor_flash_driver_write)(ULONG *flash_address, ULONG *source, ULONG words);
//UINT                            (*lx_nor_flash_driver_block_erase)(ULONG block, ULONG erase_count);
//UINT                            (*lx_nor_flash_driver_block_erased_verify)(ULONG block);
//UINT                            (*lx_nor_flash_driver_system_error)(UINT error_code);

/* Driver auxiliary functions*/
UINT _driver_qspi_init(void);
UINT _driver_nor_flash_wait_eop(ULONG);
UINT _driver_nor_flash_write_enable(void);
UINT _driver_nor_flash_configure(void);
UINT _driver_nor_flash_page_prog(ULONG address, UCHAR* data, ULONG size);
UINT _driver_test_block(ULONG block);

UINT compare_buffers(uint8_t *dst, uint8_t *src, uint32_t size);


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
    instance->lx_nor_flash_words_per_block  = DRIVER_BLOCK_SIZE / sizeof(ULONG);

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


    // Проверим все 256 блоков
    for (uint16_t blk = 0; blk < 256; blk++)
    {
       uint8_t ret = _driver_test_block(blk);
       printf("BLSTS: %d\r\n", ret);
    }

    while(1);

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
    if ((ULONG)flash_address > DRIVER_HIGHER_ADDRESS_FLASH_MEMORY || (ULONG)flash_address < DRIVER_LOWER_ADDRESS_FLASH_MEMORY)
    {
        _driver_nor_flash_system_error(LX_ERROR); // IO error !
    }
    else
    {
        flash_address = flash_address - DRIVER_BASE_OFFSET_MEM / sizeof(ULONG); // Удаляем фейковый оффсет адреса памяти
    }

    ULONG address = (ULONG)flash_address; // Extract address from pointer
    ULONG size = words * sizeof(ULONG);   // Calculate size in bytes
    UCHAR *data = (UCHAR*)source;         // Byte pointer to source data

    // Остаток байт вмещающихся до границы программируемой страницы
    ULONG temp_prog_size = N25_PAGE_PROG_SIZE - (address % N25_PAGE_PROG_SIZE);
    ULONG temp_prog_addr = address;        // Счетчик адреса
    ULONG end_address    = address + size; // Адрес байта следующего за последним байтом запрограмированных данных

    // Если свободного места больше чем у нас имеется данных для записи
    if (size < temp_prog_size)
    {
        /*
         * Когда размер программируемых данных меньше пустой области до конца
         * программируемой странице, функция программирования страницы будет
         * вызвана один раз только для программирования этого кусочка данных.
         * Поэтому программируемый размер данных составит ровно столько, сколько
         * передано в функцию.
         */
        temp_prog_size = size;
    }

    do
    {
        // Выполняем программирование с учетом полученного выравнивающего количества данных для первой страницы
        if (_driver_nor_flash_page_prog(temp_prog_addr, data, temp_prog_size) != LX_SUCCESS)
        {
            return LX_ERROR;
        }

        // Увеличиваем счетчики
        temp_prog_addr += temp_prog_size;
        data           += temp_prog_size;

        // Если до конца памяти осталось меньше чем размер программируемой страницы
        if ((temp_prog_addr + N25_PAGE_PROG_SIZE) > end_address)
        {
            // Последний чанк есть разница между адресами конца и текущего
            temp_prog_size = end_address - temp_prog_addr;
        }
        else
        {
            // В противном случае размер равен полному блоку программируемой страницы
            temp_prog_size = N25_PAGE_PROG_SIZE;
        }
    }
    while(temp_prog_addr < end_address);

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
    if ((ULONG)flash_address > DRIVER_HIGHER_ADDRESS_FLASH_MEMORY || (ULONG)flash_address < DRIVER_LOWER_ADDRESS_FLASH_MEMORY)
    {
        // IO error !
        _driver_nor_flash_system_error(LX_ERROR);
    }
    else
    {
        // Extract offset
        flash_address = flash_address - DRIVER_BASE_OFFSET_MEM / sizeof(ULONG);
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

    MODIFY_REG(QSPIHandle.Instance->DCR, QUADSPI_DCR_CSHT, QSPI_CS_HIGH_TIME_2_CYCLE);

    if (HAL_QSPI_Receive_DMA(&QSPIHandle, (uint8_t*)destination) != HAL_OK)
    {
        return LX_ERROR;
    }

    // Waiting EOR
    while(RxCplt == 0);
    RxCplt = 0;

    /* Restore S# timing for nonRead commands */
    MODIFY_REG(QSPIHandle.Instance->DCR, QUADSPI_DCR_CSHT, QSPI_CS_HIGH_TIME_5_CYCLE);

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

    QSPI_CommandTypeDef cmd;

    /* Erasing Sequence -------------------------------------------------- */
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    cmd.Instruction         = SECTOR_ERASE_CMD;
    cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
    cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
    cmd.DataMode            = QSPI_DATA_NONE;
    cmd.DummyCycles         = 0;

    // Write enable latch set
    if (_driver_nor_flash_write_enable() != LX_SUCCESS)
    {
        return LX_ERROR; // Exit if error occured
    }

    // Calculate block address
    cmd.Address = block * DRIVER_BLOCK_SIZE;

    // Send ERASE command
    if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    // Wait end of end ERASE subsector sequence
    if (_driver_nor_flash_wait_eop(N25Q128A_SUBSECTOR_ERASE_MAX_TIME) != LX_SUCCESS)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
}


/**
 * @brief Verify block erase
 *
 * @param block : Number of block (SUBSECTOR for this flash memory)
 * @return block erase status
 */
UINT _driver_nor_flash_erased_verify(ULONG block)
{
    // Block start address calculate
    ULONG curr_addr = block * DRIVER_BLOCK_SIZE;
    ULONG end_addr  = curr_addr + DRIVER_BLOCK_SIZE;

    do
    {
        // Считываем первый 512 байтовую часть подсектора
        if (_driver_nor_flash_read((ULONG*)(curr_addr + DRIVER_BASE_OFFSET_MEM), verify_sector_buffer, LX_NOR_SECTOR_SIZE) != LX_SUCCESS)
        {
            return LX_ERROR;
        }

        // Проверяем что весь сектор очищен
        for (ULONG i = 0; i < LX_NOR_SECTOR_SIZE; i++)
        {
            if (verify_sector_buffer[i] != 0xFFFFFFFF)
            {
                return LX_ERROR;
            }
        }

        // Переходим к следующему сектору
        curr_addr += (LX_NOR_SECTOR_SIZE * sizeof(ULONG));
    }
    while(curr_addr < end_addr);


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
    __NOP();

    printf("ERROR!!!");

//    _driver_nor_flash_bulk_erase();

    if (error_code == 91)
    {
    }

    return LX_SUCCESS;
}


/** AUXILLARY FUNCTIONS FOR DRIVER **/

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
    if (_driver_nor_flash_wait_eop(N25Q128A_DEFAULT_TIMEOUT) != LX_SUCCESS)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
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

    QSPIHandle.Init.ClockPrescaler = 1;
    QSPIHandle.Init.FifoThreshold = 1;
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
UINT _driver_nor_flash_wait_eop(ULONG timeout)
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

    if (HAL_QSPI_AutoPolling(&QSPIHandle, &cmd, &cfg, timeout) != HAL_OK)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;
}


/**
 * @brief Perform BULK erase
 *
 * @return status of opertaion
 */
UINT _driver_nor_flash_bulk_erase()
{
    QSPI_CommandTypeDef cmd;

    if (_driver_nor_flash_write_enable() != LX_SUCCESS)
    {
        return LX_ERROR;
    }

    /* Sent BULK erase command */
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction = BULK_ERASE_CMD;
    cmd.AddressMode = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode = QSPI_DATA_NONE;
    cmd.DummyCycles = 0;
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(&QSPIHandle, &cmd, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }


    if(_driver_nor_flash_wait_eop(N25Q128A_BULK_ERASE_MAX_TIME) != LX_SUCCESS)
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
    cfg.Interval        = 0x10;
    cfg.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling(&QSPIHandle, &cmd, &cfg, N25Q128A_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return LX_ERROR;
    }

    return LX_SUCCESS;

}

/**
 * @brief Проверка стирания, записи и удержания информации в секторе (64 KByte)
 *
 * @param  block        : Номер блока (0 ~ 255)
 * @return test_status  : Результат тестирования
 */
UINT _driver_test_block(ULONG block)
{
    // Базовый адрес блока флеш памяти с фейковым оффсетом
    ULONG block_addr = block * DRIVER_BLOCK_SIZE + DRIVER_BASE_OFFSET_MEM;

    // Полностью стираем блок
    if (_driver_nor_flash_block_erase(block, 0) != LX_SUCCESS)
    {
        return ERASE_FAILED;
    }

    // Проверяем корректность стирания блока
    if (_driver_nor_flash_erased_verify(block))
    {
        return ERASE_VERIFY_FAILED;
    }

    // Записываем 64 килобайта информации (по 512 байт)
    for (ULONG temp_addr = block_addr; temp_addr < (block_addr + DRIVER_BLOCK_SIZE); temp_addr += sizeof(data_pattern))
    {
        if (_driver_nor_flash_write((ULONG*)temp_addr, (ULONG*)&data_pattern[0], sizeof(data_pattern)/4) != LX_SUCCESS)
        {
            return WRITING_FAILED;
        }
    }

    // Проверяем что записалось
    for (ULONG temp_addr = block_addr; temp_addr < (block_addr + DRIVER_BLOCK_SIZE); temp_addr += sizeof(data_pattern))
    {
        if (_driver_nor_flash_read((ULONG*)temp_addr, &verify_sector_buffer[0], sizeof(verify_sector_buffer)/4) != LX_SUCCESS)
        {
            return READING_FAILED;
        }

        // Верифицируем блок данных
        if (compare_buffers((uint8_t*)&verify_sector_buffer[0], (uint8_t*)&data_pattern[0], sizeof(data_pattern)) != 0)
        {
            return COMPARE_FAILED;
        }
    }

    // Тесты пройдены успешно!
    return PASSED;
}

/**
 * @brief Сравнить буферы данных
 *
 * @param dst
 * @param src
 * @param size
 * @return
 */
UINT compare_buffers(uint8_t *dst, uint8_t *src, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        if (dst[i] != src[i])
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Perform test read/write driver functionality
 *
 */
void _driver_all_flash_verify()
{
    for (uint32_t i = 0; i < 4096; i++)
    {
        if (_driver_nor_flash_erased_verify(i) != LX_SUCCESS)
        {

            while(1);
        }
    }


    while(1);
}



void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    RxCplt = 1;
}

void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    TxCplt = 1;
}

void HAL_QSPI_CmdCpltCallback(QSPI_HandleTypeDef *hqspi)
{
    CmdCplt = 1;
}

//                // Если в следующей итерации цикла мы выйдем за пределы программируемой памяти
//                if ((temp_prog_addr + N25_PAGE_PROG_SIZE) > end_address)
//                {
//                    // Дописываем остаток байт в следующей команде
//                    temp_prog_size = end_address - temp_prog_addr;
//                }
//                else
//                {
//                    // Иначе в следующей итерации программируем полную страницу
//                    temp_prog_size = N25_PAGE_PROG_SIZE;
//                }

