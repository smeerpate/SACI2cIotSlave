#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h> // needed for memset

#include "SACSerial.h"


static struct termios sTio;
static int iTtyFd = 0;


int serialInit()
{
    memset(&sTio, 0, sizeof(sTio));
    sTio.c_iflag = 0;
    sTio.c_oflag = 0;
    sTio.c_cflag = CS8 | CREAD | CLOCAL;           // 8n1, see termios.h for more information
    sTio.c_lflag = 0;
    sTio.c_cc[VMIN] = 4;
    sTio.c_cc[VTIME] = 5;
    
    iTtyFd = open("/dev/ttyACM0", O_RDWR);        // O_NONBLOCK might override VMIN and VTIME, so read() may return immediately.
    cfsetospeed(&sTio, B115200);            // 115200 baud, B115200, B57600, or B9600
    cfsetispeed(&sTio, B115200);            // 115200 baud
    
    return 0;
}


int serialXfer(volatile serial_xfer_t *xfer)
{
    unsigned char c;
    
    if(!iTtyFd)
        return -1;
        
    if (read(iTtyFd,&c,1)>0)
        printf("Serial read: 0x%02x\n", c);
        
    return 0;
}

int serialTerminate()
{
    if(iTtyFd)
        close(iTtyFd);
        
    return 0;
}


/*
int main(int argc,char** argv)
{
        struct termios tio;
        struct termios stdio;
        int tty_fd;
        fd_set rdset;

        unsigned char c='D';

        printf("Please start with %s /dev/ttyS1 (for example)\n",argv[0]);
        memset(&stdio,0,sizeof(stdio));
        stdio.c_iflag=0;
        stdio.c_oflag=0;
        stdio.c_cflag=0;
        stdio.c_lflag=0;
        stdio.c_cc[VMIN]=1;
        stdio.c_cc[VTIME]=0;
        tcsetattr(STDOUT_FILENO,TCSANOW,&stdio);
        tcsetattr(STDOUT_FILENO,TCSAFLUSH,&stdio);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);       // make the reads non-blocking

        memset(&tio,0,sizeof(tio));
        tio.c_iflag=0;
        tio.c_oflag=0;
        tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
        tio.c_lflag=0;
        tio.c_cc[VMIN]=1;
        tio.c_cc[VTIME]=5;

        tty_fd=open(argv[1], O_RDWR | O_NONBLOCK);        // O_NONBLOCK might override VMIN and VTIME, so read() may return immediately.
        cfsetospeed(&tio,B115200);            // 115200 baud
        cfsetispeed(&tio,B115200);            // 115200 baud

        tcsetattr(tty_fd,TCSANOW,&tio);
        while (c!='q')
        {
                if (read(tty_fd,&c,1)>0)        write(STDOUT_FILENO,&c,1);              // if new data is available on the serial port, print it out
                if (read(STDIN_FILENO,&c,1)>0)  write(tty_fd,&c,1);                     // if new data is available on the console, send it to the serial port
        }

        close(tty_fd);
}
*/