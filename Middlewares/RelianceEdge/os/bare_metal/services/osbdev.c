/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2024 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/reliance-edge/ for more information.
 */
/** @file
    @brief Implements block device I/O.
 */
#include <redfs.h>
#include <redvolume.h>
#include <redbdev.h>

#include "lx_api.h"
#include "nor_driver.h"


/* NOR QSPI memory desc */
LX_NOR_FLASH nor_mem_desc = {0};

/* Aligned buffer (for unaligned buffer writing/reading) (uint32_t) */
ULONG ulBuffer[LX_NOR_SECTOR_SIZE] __attribute__((aligned(4))) = {0};

/* Low level init status */
enum {LX_NOINIT, LX_INIT, LX_INITERR} ini_sts = LX_NOINIT;


/** @brief Configure a block device.

    In some operating environments, block devices need to be configured with
    run-time context information that is only available at higher layers.
    For example, a block device might need to be associated with a block
    device handle or a device string.  This API allows that OS-specific
    context information to be passed down from the higher layer (e.g., a
    VFS implementation) to the block device OS service, which can save it
    for later use.

    Not all OS ports will call RedOsBDevConfig().  If called, it will be called
    while the block device is closed, prior to calling RedOsBDevOpen().

    @param bVolNum  The volume number of the volume to configure.
    @param context  OS-specific block device context information.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is not a valid volume number.
 */
REDSTATUS RedOsBDevConfig(
        uint8_t     bVolNum,
        REDBDEVCTX  context)
{
    /* Validate input parameters */
    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        return -RED_EINVAL;
    }

    /* Not used in this implementation */

    /* All operations success */
    return 0;
}


/** @brief Initialize a block device.

    This function is called when the file system needs access to a block
    device.

    Upon successful return, the block device should be fully initialized and
    ready to service read/write/flush/close requests.

    The behavior of calling this function on a block device which is already
    open is undefined.

    @param bVolNum  The volume number of the volume whose block device is being
                    initialized.
    @param mode     The open mode, indicating the type of access required.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
    @retval -RED_EIO    A disk I/O error occurred.
 */
REDSTATUS RedOsBDevOpen(
        uint8_t         bVolNum,
        BDEVOPENMODE    mode)
{
    /* Validate input parameters */
    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        return -RED_EINVAL;
    }

    /* Prevent reinitialize if we have already initialized LevelX module*/
    if (ini_sts == LX_INIT)
    {
        /* All operations success */
        return 0;
    }

    /* LevelX FTL module initialize */
    _lx_nor_flash_initialize();

    /* Initialize QSPI low level & nor flash*/
    if (_lx_nor_flash_open(&nor_mem_desc, gaRedVolConf[0].pszPathPrefix, flash_driver_init) != LX_SUCCESS)
    {
        /* Error in initialization */
        ini_sts = LX_INITERR;
        return -RED_EIO;
    }

    /* Change init status */
    ini_sts = LX_INIT;

    /* All operations success */
    return 0;
}


/** @brief Uninitialize a block device.

    This function is called when the file system no longer needs access to a
    block device.  If any resource were allocated by RedOsBDevOpen() to service
    block device requests, they should be freed at this time.

    Upon successful return, the block device must be in such a state that it
    can be opened again.

    The behavior of calling this function on a block device which is already
    closed is undefined.

    @param bVolNum  The volume number of the volume whose block device is being
                    uninitialized.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
 */
REDSTATUS RedOsBDevClose(
        uint8_t     bVolNum)
{
    /* Validate input parameters */
    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        return -RED_EINVAL;
    }

    /* Perform closing flash driver */
    if (_lx_nor_flash_close(&nor_mem_desc) != LX_SUCCESS)
    {
        ini_sts = LX_INITERR;
        return -RED_EIO;
    }

    /* Deinitialize QSPI peripheral */
    if (flash_driver_deinit() != LX_SUCCESS)
    {
        ini_sts = LX_INITERR;
        return -RED_EIO;
    }

    /* Change init status */
    ini_sts = LX_NOINIT;

    /* All operations success */
    return 0;
}


/** @brief Return the block device geometry.

    The behavior of calling this function is undefined if the block device is
    closed.

    @param bVolNum  The volume number of the volume whose block device geometry
                    is being queried.
    @param pInfo    On successful return, populated with the geometry of the
                    block device.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0               Operation was successful.
    @retval -RED_EINVAL     @p bVolNum is an invalid volume number, or @p pInfo
                            is `NULL`.
    @retval -RED_EIO        A disk I/O error occurred.
    @retval -RED_ENOTSUPP   The geometry cannot be queried on this block device.
 */
REDSTATUS RedOsBDevGetGeometry(
        uint8_t     bVolNum,
        BDEVINFO   *pInfo)
{
    if((bVolNum >= REDCONF_VOLUME_COUNT) || (pInfo == NULL))
    {
        return -RED_EINVAL;
    }

    /* Get sector count */
    pInfo->ullSectorCount = nor_mem_desc.lx_nor_flash_total_physical_sectors;

    /* Get sector size */
    pInfo->ulSectorSize = LX_NOR_SECTOR_SIZE * sizeof(ULONG);

    /* All operations success*/
    return 0;
}


/** @brief Read sectors from a physical block device.

    The behavior of calling this function is undefined if the block device is
    closed or if it was opened with ::BDEV_O_WRONLY.

    @param bVolNum          The volume number of the volume whose block device
                            is being read from.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to read.
    @param pBuffer          The buffer into which to read the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number, @p pBuffer is
                        `NULL`, or @p ullStartSector and/or @p ulSectorCount
                        refer to an invalid range of sectors.
    @retval -RED_EIO    A disk I/O error occurred.
 */
REDSTATUS RedOsBDevRead(
        uint8_t     bVolNum,
        uint64_t    ullSectorStart,
        uint32_t    ulSectorCount,
        void       *pBuffer)
{
    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
            || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
            || (pBuffer == NULL))
    {
        return -RED_EINVAL;
    }

    /* Copy data pointer */
    uint8_t *tmpBuf = pBuffer;

    /* Copy start sector index */
    uint64_t ullTmpSector = ullSectorStart;

    /* Read ulSectorCount (512 bytes block) */
    for (uint32_t ulCnt = 0; ulCnt < ulSectorCount; ulCnt++)
    {
        /* If pointer is aligned */
        if (IS_ALIGNED_PTR(pBuffer, sizeof(uint32_t)))
        {
            /* Read 512 byte logical sector */
            if (_lx_nor_flash_sector_read(&nor_mem_desc, ullTmpSector, tmpBuf) != LX_SUCCESS)
            {
                return -RED_EIO;
            }
        }
        else
        {
            /* Read 512 byte logical sector to aligned buffer */
            if (_lx_nor_flash_sector_read(&nor_mem_desc, ullTmpSector, ulBuffer) != LX_SUCCESS)
            {
                return -RED_EIO;
            }

            /* Copy to unaligned buffer */
            RedMemCpy(tmpBuf, ulBuffer, sizeof(ulBuffer));
        }

        /* Increase sector counter */
        ullTmpSector += 1;

        /* Shift reading buffer pointer */
        tmpBuf += 512;
    }

    /* All operations success */
    return 0;
}


#if REDCONF_READ_ONLY == 0
/** @brief Write sectors to a physical block device.

    The behavior of calling this function is undefined if the block device is
    closed or if it was opened with ::BDEV_O_RDONLY.

    @param bVolNum          The volume number of the volume whose block device
                            is being written to.
    @param ullSectorStart   The starting sector number.
    @param ulSectorCount    The number of sectors to write.
    @param pBuffer          The buffer from which to write the sector data.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number, @p pBuffer is
                        `NULL`, or @p ullStartSector and/or @p ulSectorCount
                        refer to an invalid range of sectors.
    @retval -RED_EIO    A disk I/O error occurred.
 */
REDSTATUS RedOsBDevWrite(
        uint8_t     bVolNum,
        uint64_t    ullSectorStart,
        uint32_t    ulSectorCount,
        const void *pBuffer)
{
    /* Verify input arguments */
    if(    (bVolNum >= REDCONF_VOLUME_COUNT)
            || !VOLUME_SECTOR_RANGE_IS_VALID(bVolNum, ullSectorStart, ulSectorCount)
            || (pBuffer == NULL))
    {
        return -RED_EINVAL;
    }

    /* Copy data pointer */
    uint8_t *tmpBuf = pBuffer;

    /* Copy start sector index */
    uint64_t ullTmpSector = ullSectorStart;

    /* Write ulSectorCount (512 bytes block) */
    for (uint32_t ulCnt = 0; ulCnt < ulSectorCount; ulCnt++)
    {
        /* If pointer is aligned */
        if (IS_ALIGNED_PTR(tmpBuf, sizeof(uint32_t)))
        {
            /* Write 512 byte logical sector */
            if (_lx_nor_flash_sector_write(&nor_mem_desc, ullTmpSector, tmpBuf) != LX_SUCCESS)
            {
                return -RED_EIO;
            }
        }
        else
        {
            /* Copy data to aligned buffer */
            RedMemCpy(ulBuffer, tmpBuf, sizeof(ulBuffer));

            /* Write 512 byte logical sector from aligned buffer */
            if (_lx_nor_flash_sector_write(&nor_mem_desc, ullTmpSector, ulBuffer) != LX_SUCCESS)
            {
                return -RED_EIO;
            }
        }

        /* Increase sector counter */
        ullTmpSector += 1;

        /* Shift writing buffer */
        tmpBuf += 512;
    }

    /* All operations success */
    return 0;
}


/** @brief Flush any caches beneath the file system.

    This function must synchronously flush all software and hardware caches
    beneath the file system, ensuring that all sectors written previously are
    committed to permanent storage.

    If the environment has no caching beneath the file system, the
    implementation of this function can do nothing and return success.

    The behavior of calling this function is undefined if the block device is
    closed or if it was opened with ::BDEV_O_RDONLY.

    @param bVolNum  The volume number of the volume whose block device is being
                    flushed.

    @return A negated ::REDSTATUS code indicating the operation result.

    @retval 0           Operation was successful.
    @retval -RED_EINVAL @p bVolNum is an invalid volume number.
    @retval -RED_EIO    A disk I/O error occurred.
 */
REDSTATUS RedOsBDevFlush(
        uint8_t     bVolNum)
{
    /* Verify input arguments */
    if(bVolNum >= REDCONF_VOLUME_COUNT)
    {
        return -RED_EINVAL;
    }

    /* Not used in this implementation*/

    /* All operations success */
    return 0;
}
#endif /* REDCONF_READ_ONLY == 0 */

