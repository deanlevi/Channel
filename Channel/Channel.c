#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "Channel.h"

#define LOCAL_PORT_NUM_ARGUMENT_INDEX 1
#define RECEIVER_IP_ADDRESS_ARGUMENT_INDEX 2
#define RECEIVER_PORT_NUM_ARGUMENT_INDEX 3
#define ERROR_PROBABILITY_ARGUMENT_INDEX 4
#define RANDOM_SEED_ARGUMENT_INDEX 5
#define SOCKET_PROTOCOL 0
#define BINDING_SUCCEEDED 0
#define SEND_RECEIVE_FLAGS 0
#define CHUNK_SIZE 8
#define MESSAGE_LENGTH 20
#define SEND_MESSAGES_WAIT 20

/*
Input: argv - to update input parameters.
Output: none.
Description: update channel parameters and init variables.
*/
void InitChannel(char *argv[]);

/*
Input: none.
Output: none.
Description: bind channel socket to requested port.
*/
void BindToPort();

/*
Input: none.
Output: none.
Description: handle operation of the channel. receive from sender, insert errors and send to receiver, and also receive
			 ending message from receiver and delivering it to sender.
*/
void HandleTraffic();

/*
Input: ReceivedBuffer - received buffer of data from sender, ReceivedBufferLength - length of received buffer in bytes.
Output: none.
Description: insert errors to senders data and send it to receiver.
*/
void HandleReceiveFromSender(unsigned long long ReceivedBuffer, int ReceivedBufferLength);

/*
Input: ReceivedBuffer - received buffer of data from sender.
Output: none.
Description: insert errors to senders data by flipping each bit by given probability.
			 for each bit a random number of 16 bits is created. if the random number is smaller than n (= ErrorProbability),
			 meaning we hit a chance of n/2^16 -> flip bit.
			 since rand() creates a random number of 15 bits, we used TempRandomNumber, TempRandomNumberSize to create another random
			 number and use one random bit from it to each calculation.
			 meaning an extra rand() for each 15 data bits (= 1.0667 rand() for each data bit).
*/
void InsertErrors(unsigned long long *ReceivedBuffer);

/*
Input: ReceivedBuffer - received buffer of data from sender, ReceivedBufferLength - length of received buffer in bytes.
Output: none.
Description: sending ending message from receiver to sender.
*/
void HandleReceiveFromReceiver(char *MessageFromReceiver, int ReceivedBufferLength);

/*
Input: none.
Output: none.
Description: close sockets and wsa data at the end of the channel's operation.
*/
void CloseSocketsAndWsaData();

void InitChannel(char *argv[]) {
	Channel.LocalPortNum = atoi(argv[LOCAL_PORT_NUM_ARGUMENT_INDEX]);
	Channel.ReceiverIPAddress = argv[RECEIVER_IP_ADDRESS_ARGUMENT_INDEX];
	Channel.ReceiverPortNum = atoi(argv[RECEIVER_PORT_NUM_ARGUMENT_INDEX]);
	Channel.ErrorProbability = atoi(argv[ERROR_PROBABILITY_ARGUMENT_INDEX]);
	Channel.RandomSeed = atoi(argv[RANDOM_SEED_ARGUMENT_INDEX]);
	Channel.ReceiverSocketService.sin_family = AF_INET;
	Channel.ReceiverSocketService.sin_addr.s_addr = inet_addr(Channel.ReceiverIPAddress);
	Channel.ReceiverSocketService.sin_port = htons(Channel.ReceiverPortNum);
	srand(Channel.RandomSeed);
	Channel.NumberOfReceivedBytes = 0;
	Channel.NumberOfFlippedBits = 0;
	Channel.TempRandomNumberSize = 0;

	WSADATA wsaData;
	int StartupRes = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (StartupRes != NO_ERROR) {
		fprintf(stderr, "Error %ld at WSAStartup().\nExiting...\n", StartupRes);
		exit(ERROR_CODE);
	}

	Channel.ReceiverSocket = socket(AF_INET, SOCK_DGRAM, SOCKET_PROTOCOL);
	if (Channel.ReceiverSocket == INVALID_SOCKET) {
		fprintf(stderr, "InitChannel failed to create ReceiverSocket. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
	Channel.ChannelSocket = socket(AF_INET, SOCK_DGRAM, SOCKET_PROTOCOL);
	if (Channel.ChannelSocket == INVALID_SOCKET) {
		fprintf(stderr, "InitChannel failed to create ChannelSocket. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
}

void BindToPort() {
	int BindingReturnValue;
	Channel.ChannelSocketService.sin_family = AF_INET;
	Channel.ChannelSocketService.sin_addr.s_addr = INADDR_ANY;
	Channel.ChannelSocketService.sin_port = htons(Channel.LocalPortNum);
	BindingReturnValue = bind(Channel.ChannelSocket, (SOCKADDR*)&Channel.ChannelSocketService,
		sizeof(Channel.ChannelSocketService));
	if (BindingReturnValue != BINDING_SUCCEEDED) {
		fprintf(stderr, "BindToPort failed to bind.\n");
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
}

void HandleTraffic() {
	fd_set Allfds;
	fd_set Readfds;
	int Status;
	FD_ZERO(&Allfds);
	FD_SET(Channel.ReceiverSocket, &Allfds);
	FD_SET(Channel.ChannelSocket, &Allfds);

	unsigned long long ReceivedBuffer;
	char MessageFromReceiver[MESSAGE_LENGTH];
	int FromLen = sizeof(Channel.SenderSocketService);
	int ReceivedBufferLength = recvfrom(Channel.ChannelSocket, &ReceivedBuffer, CHUNK_SIZE, SEND_RECEIVE_FLAGS,
									   (SOCKADDR*)&Channel.SenderSocketService, &FromLen);
	if (ReceivedBufferLength == SOCKET_ERROR) {
		fprintf(stderr, "HandleTraffic failed to recvfrom. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
	Channel.NumberOfReceivedBytes += ReceivedBufferLength;
	HandleReceiveFromSender(ReceivedBuffer, ReceivedBufferLength);

	while (TRUE) {
		Readfds = Allfds;
		Status = select(0, &Readfds, NULL, NULL, NULL);
		if (Status == SOCKET_ERROR) {
			fprintf(stderr, "HandleTraffic select failure. Error Number is %d\n", WSAGetLastError());
			CloseSocketsAndWsaData();
			exit(ERROR_CODE);
		}
		else if (Status == 0) {
			continue;
		}
		else {
			if (FD_ISSET(Channel.ChannelSocket, &Readfds)) {
				ReceivedBufferLength = recvfrom(Channel.ChannelSocket, &ReceivedBuffer, CHUNK_SIZE, SEND_RECEIVE_FLAGS, NULL, NULL);
				if (ReceivedBufferLength == SOCKET_ERROR) {
					fprintf(stderr, "HandleTraffic failed to recvfrom. Error Number is %d\n", WSAGetLastError());
					CloseSocketsAndWsaData();
					exit(ERROR_CODE);
				}
				Channel.NumberOfReceivedBytes += ReceivedBufferLength;
				HandleReceiveFromSender(ReceivedBuffer, ReceivedBufferLength);
			}
			if (FD_ISSET(Channel.ReceiverSocket, &Readfds)) {
				ReceivedBufferLength = recvfrom(Channel.ReceiverSocket, MessageFromReceiver, MESSAGE_LENGTH, SEND_RECEIVE_FLAGS, NULL, NULL);
				if (ReceivedBufferLength == SOCKET_ERROR) {
					fprintf(stderr, "HandleTraffic failed to recvfrom. Error Number is %d\n", WSAGetLastError());
					CloseSocketsAndWsaData();
					exit(ERROR_CODE);
				}
				HandleReceiveFromReceiver(MessageFromReceiver, ReceivedBufferLength);
				break;
			}
		}
	}
	fprintf(stderr, "sender: %s\n", inet_ntoa(Channel.SenderSocketService.sin_addr));
	fprintf(stderr, "receiver: %s\n", Channel.ReceiverIPAddress);
	fprintf(stderr, "%d bytes, flipped %d bits\n", Channel.NumberOfReceivedBytes, Channel.NumberOfFlippedBits);
}

void HandleReceiveFromSender(unsigned long long ReceivedBuffer, int ReceivedBufferLength) {
	InsertErrors(&ReceivedBuffer);
	int SentBufferLength = sendto(Channel.ReceiverSocket, &ReceivedBuffer, ReceivedBufferLength, SEND_RECEIVE_FLAGS,
								 (SOCKADDR*)&Channel.ReceiverSocketService, sizeof(Channel.ReceiverSocketService));
	if (SentBufferLength == SOCKET_ERROR) {
		fprintf(stderr, "HandleReceiveFromSender failed to sendto. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
}

void InsertErrors(unsigned long long *ReceivedBuffer) { // todo check bonus
	if (Channel.ErrorProbability == 0) {
		return;
	}
	int RandomNumber;

	int IndexInBuffer = 0;
	unsigned long long ErrorMask = 0;
	int NumberOfErrors = 0;
	for (; IndexInBuffer < (CHUNK_SIZE * 8); IndexInBuffer++) {
		RandomNumber = rand(); // 15 bit random number
		if (Channel.TempRandomNumberSize == 0) { // if need to create another extra random number
			Channel.TempRandomNumber = rand();
			Channel.TempRandomNumberSize = 15;
		}
		RandomNumber = RandomNumber * 2 + (Channel.TempRandomNumber & 1); // 16 bit random number, take LSB of Channel.TempRandomNumber
		Channel.TempRandomNumber = Channel.TempRandomNumber / 2; // remove used random bit
		Channel.TempRandomNumberSize--; // update size of Channel.TempRandomNumber

		if (RandomNumber < Channel.ErrorProbability) { // if we hit 0...n - 1 = n options out of 2^16
			ErrorMask += 1;
			NumberOfErrors++;
		}
		ErrorMask = ErrorMask << 1;
	}
	*ReceivedBuffer = *ReceivedBuffer ^ ErrorMask;
	Channel.NumberOfFlippedBits += NumberOfErrors;
}

void HandleReceiveFromReceiver(char *MessageFromReceiver, int ReceivedBufferLength) {
	int SentBufferLength = sendto(Channel.ChannelSocket, MessageFromReceiver, ReceivedBufferLength, SEND_RECEIVE_FLAGS,
								 (SOCKADDR*)&Channel.SenderSocketService, sizeof(Channel.SenderSocketService));
	if (SentBufferLength == SOCKET_ERROR) {
		fprintf(stderr, "HandleReceiveFromReceiver failed to sendto. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
	Sleep(SEND_MESSAGES_WAIT);
}

void CloseSocketsAndWsaData() {
	int CloseSocketReturnValue;
	CloseSocketReturnValue = closesocket(Channel.ReceiverSocket);
	if (CloseSocketReturnValue == SOCKET_ERROR) {
		fprintf(stderr, "CloseSocketsAndWsaData failed to close ListeningToReceiverSocket. Error Number is %d\n", WSAGetLastError());
		exit(ERROR_CODE);
	}
	CloseSocketReturnValue = closesocket(Channel.ChannelSocket);
	if (CloseSocketReturnValue == SOCKET_ERROR) {
		fprintf(stderr, "CloseSocketsAndWsaData failed to close ListeningToSenderSocket. Error Number is %d\n", WSAGetLastError());
		exit(ERROR_CODE);
	}
	if (WSACleanup() == SOCKET_ERROR) {
		fprintf(stderr, "CloseSocketsAndWsaData Failed to close Winsocket, error %ld. Ending program.\n", WSAGetLastError());
		exit(ERROR_CODE);
	}
}