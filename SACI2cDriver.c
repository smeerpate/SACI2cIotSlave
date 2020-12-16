#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "SACI2cDriver.h"

#define BCM2708_PERI_BASE       0x3f000000
#define SPI_BSC_SLAVE_BASE      (BCM2708_PERI_BASE + 0x214000) // Offset 0x214000 = SPI/BSC slave. 
#define SPI_BSC_SLAVE_NREG      16
#define GPIO_BASE               (BCM2708_PERI_BASE + 0x200000)

#define PAGE_SIZE (4*1024)

/* individual SPI_BSC_SLAVE register offsets */
#define SPI_BSC_SLAVE_DR        0x00 // Data register */
#define SPI_BSC_SLAVE_RSR       0x04 // Operation status register and error clear register */
#define SPI_BSC_SLAVE_SLV       0x08 // I2C slave address register */
#define SPI_BSC_SLAVE_CR        0x0c // Control register to configure the i2c/spi operation */
#define SPI_BSC_SLAVE_FR        0x10 // Flag register */
#define SPI_BSC_SLAVE_IFLS      0x14 // Interrupt fifo level select register */
#define SPI_BSC_SLAVE_IMSC      0x18 // Interrupt mask set clear register */
#define SPI_BSC_SLAVE_RIS       0x1c // Raw interrupt status register */
#define SPI_BSC_SLAVE_MIS       0x20 // Masked interrupt status register */
#define SPI_BSC_SLAVE_ICR       0x24 // Interrupt clear register */
#define SPI_BSC_SLAVE_DMACR     0x28 // DMA control register */
#define SPI_BSC_SLAVE_TDR       0x2c // Fifo test data register */
#define SPI_BSC_SLAVE_GPUSTAT   0x30 // GPU status register */
#define SPI_BSC_SLAVE_HCTRL     0x34 // Host control register */
#define SPI_BSC_SLAVE_DEBUG1    0x38 // I2C debug register */
#define SPI_BSC_SLAVE_DEBUG2    0x3c // SPI debug register */

/* individual GPIO register offsets */
#define GPIO_GPFSEL1            0x04 // GPIO function select 1 register */

/****************** private function prototypes *********************/
int periphReadReg(uint32_t uiBase, uint32_t uiOffset, uint32_t *puiData);
int periphWriteReg(uint32_t uiBase, uint32_t uiOffset, uint32_t uiData);
/********************************************************************/

/******************** private global variables **********************/

/********************************************************************/

int slavedriverSetI2cAddress(uint8_t bAddress)
{
    int iReturnValue = 0;
    tSpiBcsPeriphRegSLV sSa;
    
    sSa.address = bAddress;
    sSa.reserved = 0;
    printf("[INFO] %s: Setting slave address to 0x%x\n", __func__, (uint32_t)sSa.address);
    iReturnValue = periphWriteReg(SPI_BSC_SLAVE_BASE, SPI_BSC_SLAVE_CR, 0x305);
    iReturnValue = periphWriteReg(SPI_BSC_SLAVE_BASE, SPI_BSC_SLAVE_SLV, (uint32_t)sSa.address);
    return iReturnValue;
}

int slavedriverGetI2cAddress(uint8_t *bAddress)
{
    int iReturnValue = 0;
    tSpiBcsPeriphRegSLV sSa;
    sSa.u32 = 0;
    uint32_t val;
    
    iReturnValue = periphReadReg(SPI_BSC_SLAVE_BASE, SPI_BSC_SLAVE_CR, &val);
    iReturnValue = periphReadReg(SPI_BSC_SLAVE_BASE, SPI_BSC_SLAVE_SLV, (uint32_t *)&sSa);
    if(bAddress != NULL)
    {
        *bAddress = sSa.address;
    }
    printf("[INFO] %s: Got slave address: 0x%x, 0x%x\n", __func__, sSa.u32, val);
    return iReturnValue;
}

int slavedriverInitGpioMUX()
{
    int iReturnValue = 0;
    tGpioPeriphRegGPFSET sGpioFsel1;
    
    sGpioFsel1.u32 = 0;
    // GPIO19, ALT3 = SCL (ALT3 = 3'b111)
    sGpioFsel1.fSelx9 = 7;
    // GPIO18, ALT3 = SDA (ALT3 = 3'b111)
    sGpioFsel1.fSelx8 = 7;
    iReturnValue = periphWriteReg(GPIO_BASE, GPIO_GPFSEL1, sGpioFsel1.u32);
    return iReturnValue;
}





/*
    read a single 32 bit register.
    Offset is in bytes.
    Only works if offset < PAGE_SIZE.
    returns:
        0: ok
        -1: Can't open /dev/mem
        -2: Mmap failed
*/
int periphReadReg(uint32_t uiBase, uint32_t uiOffset, uint32_t *puiData)
{
    int iReturnValue = 0;
    int iMemFd;
    void *puiPMap;
    volatile uint32_t *pReg;
    
    // open /dev/mem in read/write mode, blocking
    if((iMemFd = open("/dev/mem", O_RDWR|O_SYNC)) < 0)
    {
        printf("[ERROR] %s: Can't open /dev/mem. (%s)\n", __func__, strerror(errno));
        return -1;
    }
   
    // mmap SFR(s)
    puiPMap = mmap(
        NULL,                   // Any adddress in our space will do
        PAGE_SIZE,              // Map length
        PROT_READ|PROT_WRITE,   // Enable writing to mapped memory
        MAP_SHARED,            // Not shared with other processes
        iMemFd,                 // File descriptor
        uiBase                  // Base address to the SFR
    );
   
    if(puiPMap == MAP_FAILED)
    {
        printf("[ERROR] %s: Mmap failed. (%s)\n", __func__, strerror(errno));
        return -2;
    }
   
    // copy the register contents
    pReg = (volatile uint32_t *)puiPMap + (uiOffset/sizeof(uint32_t));
    *puiData = *pReg;
    printf("[INFO] %s: (*0x%08x = 0x%08x)\n", __func__, (uint32_t)(uiBase + uiOffset), *puiData); 
   
    // Clean up
    munmap((void *)puiPMap, PAGE_SIZE);
    close(iMemFd);
    
    return iReturnValue;
}

/*
    Write a single 32 bit register.
    Offset is in bytes.
    Only works if offset < PAGE_SIZE.
    returns:
        0: ok
        -1: Can't open /dev/mem
        -2: Mmap failed
*/
int periphWriteReg(uint32_t uiBase, uint32_t uiOffset, uint32_t uiData)
{
    int iReturnValue = 0;
    int iMemFd;
    void *puiPMap;
    volatile uint32_t *pReg;
    
    // open /dev/mem in read/write mode, blocking
    if((iMemFd = open("/dev/mem", O_RDWR|O_SYNC)) < 0)
    {
        printf("[ERROR] %s: Can't open /dev/mem. (%s)\n", __func__, strerror(errno));
        return -1;
    }
   
    // mmap SFR(s)
    puiPMap = mmap(
        NULL,                   // Any adddress in our space will do
        PAGE_SIZE,              // Map length
        PROT_READ|PROT_WRITE,   // Enable writing to mapped memory
        MAP_SHARED,            // Not shared with other processes
        iMemFd,                 // File descriptor
        uiBase                  // Base address to the SFR
    );
   
    if(puiPMap == MAP_FAILED)
    {
        printf("[ERROR] %s: Mmap failed. (%s)\n", __func__, strerror(errno));
        return -2;
    }
   
    // copy the input data to the register
    //*((volatile uint32_t *)(puiPMap)) = *mpReg;
    pReg = (volatile uint32_t *)puiPMap + (uiOffset/sizeof(uint32_t));
    //memcpy((void *)pReg, (void *)&uiData, sizeof(uint32_t));
    *pReg = uiData;
    // Clean up
    munmap((void *)puiPMap, PAGE_SIZE);
    close(iMemFd);
    
    return iReturnValue;
}