#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#include "SACI2cDriver.h"

#define BCM2708_PERI_BASE       0x3F000000
#define SPI_BSC_SLAVE_BASE      (BCM2708_PERI_BASE + 0x214000) /* Offset 0x214000 = SPI/BSC slave. */

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