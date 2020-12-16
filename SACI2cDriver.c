#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include "SACI2cDriver.h"

#define BCM2708_PERI_BASE       0x3F000000
#define SPI_BSC_SLAVE_BASE      (BCM2708_PERI_BASE + 0x214000) /* Offset 0x214000 = SPI/BSC slave. */
#define SPI_BSC_SLAVE_NREG      16

/* individual register offsets */
#define SPI_BSC_SLAVE_DR        0x00 /* Data register */
#define SPI_BSC_SLAVE_RSR       0x04 /* Operation status register and error clear register */
#define SPI_BSC_SLAVE_SLV       0x08 /* I2C slave address register */
#define SPI_BSC_SLAVE_CR        0x0c /* Control register to configure the i2c/spi operation */
#define SPI_BSC_SLAVE_FR        0x10 /* Flag register */
#define SPI_BSC_SLAVE_IFLS      0x14 /* Interrupt fifo level select register */
#define SPI_BSC_SLAVE_IMSC      0x18 /* Interrupt mask set clear register */
#define SPI_BSC_SLAVE_RIS       0x1c /* Raw interrupt status register */
#define SPI_BSC_SLAVE_MIS       0x20 /* Masked interrupt status register */
#define SPI_BSC_SLAVE_ICR       0x24 /* Interrupt clear register */
#define SPI_BSC_SLAVE_DMACR     0x28 /* DMA control register */
#define SPI_BSC_SLAVE_TDR       0x2c /* Fifo test data register */
#define SPI_BSC_SLAVE_GPUSTAT   0x30 /* GPU status register */
#define SPI_BSC_SLAVE_HCTRL     0x34 /* Host control register */
#define SPI_BSC_SLAVE_DEBUG1    0x38 /* I2C debug register */
#define SPI_BSC_SLAVE_DEBUG2    0x3c /* SPI debug register */


/****************** private function prototypes *********************/
int periphReadReg(uint32_t uiBase, uint32_t uiOffset, uint32_t *puiData);
int periphWriteReg(uint32_t uiBase, uint32_t uiOffset, uint32_t uiData);
/********************************************************************/

int slavedriverSetI2cAddress(uint8_t bAddress)
{
    int iReturnValue = 0;
    tSpiBcsPeriphRegSLV sSa;
    
    sSa.address = bAddress;
    sSa.reserved = 0;
    printf("[INFO] %s: Setting slave address to 0x%x", __func__);
    iReturnValue = periphWriteReg(SPI_BSC_SLAVE_BASE, SPI_BSC_SLAVE_SLV, sSa.u32);
    return iReturnValue;
}

int slavedriverGetI2cAddress(uint8_t *bAddress)
{
    int iReturnValue = 0;
    tSpiBcsPeriphRegSLV sSa;
    
    iReturnValue = periphReadReg(SPI_BSC_SLAVE_BASE, SPI_BSC_SLAVE_SLV, (uint32_t *)&sSa);
    *bAddress = sSa.address;
    return iReturnValue;
}





/*
    read a single 32 bit register.
    returns:
        0: ok
        -1: Can't open /dev/mem
        -2: Mmap failed
*/
int periphReadReg(uint32_t uiBase, uint32_t uiOffset, uint32_t *puiData)
{
    int iReturnValue = 0;
    int iMemFd;
    volatile uint32_t *puiPMap;
    
    // open /dev/mem in read only mode, blocking
    if((iMemFd = open("/dev/mem", O_RDONLY|O_SYNC) ) < 0)
    {
        printf("[ERROR] %s: Can't open /dev/mem\n", __func__);
        return -1;
    }
   
    // mmap SFR(s)
    puiPMap = mmap(
        NULL,                 // Any adddress in our space will do
        sizeof(uint32_t),     // Map length
        PROT_READ,            // Enable reading from mapped memory
        MAP_PRIVATE,          // Not shared with other processes
        iMemFd,               // File to map
        uiBase+uiOffset       // Address to the SFR
    );
   
    if(puiPMap == MAP_FAILED)
    {
        printf("[ERROR] %s: Mmap failed\n", __func__);
        return -2;
    }
   
    // copy the register contents
    *puiData = *puiPMap;
   
   // Clean up
    munmap(uiPMap, bNBytes);
    close(iMemFd);
    
    return iReturnValue;
}

/*
    Write a single 32 bit register.
    returns:
        0: ok
        -1: Can't open /dev/mem
        -2: Mmap failed
*/
int periphWriteReg(uint32_t uiBase, uint32_t uiOffset, uint32_t uiData)
{
    int iReturnValue = 0;
    int iMemFd;
    volatile uint32_t *puiPMap;
    
    // open /dev/mem in write only mode, blocking
    if((iMemFd = open("/dev/mem", O_WRONLY|O_SYNC) ) < 0)
    {
        printf("[ERROR] %s: Can't open /dev/mem\n", __func__);
        return -1;
    }
   
    // mmap SFR(s)
    puiPMap = mmap(
        NULL,                 // Any adddress in our space will do
        sizeof(uint32_t),     // Map length
        PROT_WRITE,           // Enable writing to mapped memory
        MAP_PRIVATE,          // Not shared with other processes
        iMemFd,               // File to map
        uiBase+uiOffset       // Address to the SFR
    );
   
    if(puiPMap == MAP_FAILED)
    {
        printf("[ERROR] %s: Mmap failed\n", __func__);
        return -2;
    }
   
    // copy the input data to the register
    *puiPMap = *puiData;
   
   // Clean up
    munmap(uiPMap, bNBytes);
    close(iMemFd);
    
    return iReturnValue;
}