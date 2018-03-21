#ifndef CHANNEL_H
#define CHANNEL_H

#include <WinSock2.h>

#pragma comment(lib, "Ws2_32.lib")

#define ERROR_CODE (int)(-1)

typedef struct _ChannelProperties {
	int LocalPortNum;
	char *ReceiverIPAddress;
	int ReceiverPortNum;
	int ErrorProbability;
	int RandomSeed;

	SOCKET ReceiverSocket;
	SOCKADDR_IN ReceiverSocketService;

	SOCKET ChannelSocket;
	SOCKADDR_IN ChannelSocketService;

	SOCKADDR_IN SenderSocketService;
	int NumberOfReceivedBytes;
	int NumberOfFlippedBits;
}ChannelProperties;

ChannelProperties Channel;

void InitChannel(char *argv[]);
void BindToPort();
void HandleTraffic();
void CloseSocketsAndWsaData();

#endif