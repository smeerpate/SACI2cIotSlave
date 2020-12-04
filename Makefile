# https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

SACRPiIotSlave: SACRPiIotSlave.c SACServerComms.c SACPrintUtils.c
	gcc -Wall -pthread -o SACRPiIotSlave SACRPiIotSlave.c SACServerComms.c SACPrintUtils.c -lpigpio -lrt -lssl -lcrypto -I.
