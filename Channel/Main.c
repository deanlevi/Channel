#include <stdio.h>

#include "Channel.h"

#define SUCCESS_CODE 0

int main(int argc, char *argv[]) {
	if (argc != 6) {
		fprintf(stderr, "Not the right amount of input arguments.\nNeed to give two.\nExiting...\n"); // first is path, other five are inputs
		return ERROR_CODE;
	}
	InitChannel(argv);
	BindToPort();
	HandleTraffic();
	CloseSocketsAndWsaData();
	return SUCCESS_CODE;
}