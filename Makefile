# https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

SACRPiIotSlave: SACRPiIotSlave.c SACServerComms.c SACPrintUtils.c SACStructs.c SACI2cDriver.c
	gcc -Wall -pthread -o SACRPiIotSlave SACRPiIotSlave.c SACServerComms.c SACPrintUtils.c SACStructs.c SACI2cDriver.c -lpigpio -lrt -lssl -lcrypto -I.
