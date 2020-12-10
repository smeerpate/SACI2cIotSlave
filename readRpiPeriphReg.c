#define BCM2708_PERI_BASE          0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x214000) /* SPI/BSC slave */

#define BYTE_TO_BINARY_PATTERN %c%c%c%c%c%c%c%c
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *periph_map;

// I/O access
volatile unsigned int *periph;

void setup_io();

int main(int argc, char **argv)
{
  int g;

  // Set up gpi pointer for direct register access
  setup_io();
  
  printf("reg startaddress = 0x%08x\n", GPIO_BASE);

  for(g=0; g<16; g+=1)
  {
      printf("reg offset[%i] (0x%08x) = 0x%08x\n", g, GPIO_BASE + (g * 4), periph[g]);
  }

  return 0;

} // main

//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }

   /* mmap GPIO */
   periph_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (periph_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)periph_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   periph = (volatile unsigned *)periph_map;


} // setup_io