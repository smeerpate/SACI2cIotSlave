# https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

SACRPiIotSlave: SACRPiIotSlave.c SACServerComms.c SACPrintUtils.c SACStructs.c SACSerial.c
	gcc -Wall -pthread -o SACRPiIotSlave SACRPiIotSlave.c SACServerComms.c SACPrintUtils.c SACStructs.c SACSerial.c -lrt -lssl -lcrypto -I.
