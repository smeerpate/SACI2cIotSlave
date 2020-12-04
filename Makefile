# https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

SACRPiIotSlave: SACRPiIotSlave.c
	gcc -Wall -pthread -o SACRPiIotSlave SACRPiIotSlave.c -lpigpio -lrt -lssl -lcrypto
