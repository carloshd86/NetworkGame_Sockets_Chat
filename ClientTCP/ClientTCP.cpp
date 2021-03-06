// ClientTCP.cpp: define el punto de entrada de la aplicación de consola.
//

#include "stdafx.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>


#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>


using namespace std;
#define SERVER_PORT "12345"
#define BUF_SIZE 4096


struct ThreadReturn
{
	int result;
};

struct SocketParam
{
	SOCKET socketId;
};

bool clientActive = true;
bool serverActive = true;

void * writeInChat(void * param)
{
	ThreadReturn response { 0 };

	SOCKET sockfd = ((SocketParam *)param)->socketId;


	char buf[BUF_SIZE];
	memset(&buf, 0, sizeof(buf));

	bool firstMessage = true;
	printf("Please enter your nickname: ");

	while (clientActive && serverActive)
	{
		gets_s(buf, _countof(buf));
		int length = strlen(buf) + 1;
		int numBytesSend, totalSend = 0;
		do {
			numBytesSend = send(sockfd, buf, length - totalSend, 0);
			totalSend += numBytesSend;

			if (totalSend < 0) break;

		} while (totalSend < length);

		if (firstMessage) {
			firstMessage = false;
			printf("--------- Chat ---------\n");
		}

		if (2 == length && !strcmp(buf, "q"))
		{
			clientActive = false;
		}
	}

	pthread_exit(NULL);
	return &response;
}

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	int wsaerr;
	SOCKET sockfd;

	if (argc < 2) {
		return -1;
	}
	wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaerr != 0)
	{
		printf("Could not initialize Winsock.\n");
		return -1;
	}
	struct addrinfo hints;// hints de la direccion buscada
	struct addrinfo* servInfo;// contendra la lista de direcciones encontradas
	struct addrinfo* srvaddr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	// Busca las addrinfo del host al que nos queremos conectar
	int error = getaddrinfo(argv[1], SERVER_PORT, &hints, &servInfo);
	if (error != 0) { 
		printf("getaddrinfo failed"); 
		WSACleanup(); 
		return -1;
	} 

	for (srvaddr = servInfo; (srvaddr != NULL); srvaddr = srvaddr->ai_next) { 
		sockfd = socket(srvaddr->ai_family, srvaddr->ai_socktype, srvaddr->ai_protocol); 
		if (sockfd != INVALID_SOCKET) { 
			int iResult = connect(sockfd, srvaddr->ai_addr, (int)srvaddr->ai_addrlen); 
			if (iResult == SOCKET_ERROR) { closesocket(sockfd); 
			sockfd = INVALID_SOCKET; } else break; 
		} 
	} 

	freeaddrinfo(servInfo); 

	if (sockfd != INVALID_SOCKET) {

		pthread_t tid;
		SocketParam param { sockfd };

		if (!pthread_create(&tid, nullptr, writeInChat, &param)) {
			char buf[BUF_SIZE];
			memset(&buf, 0, sizeof(buf));

			int totalRecv = 0; 

			while (clientActive && serverActive) {
				int rec = 0;
				totalRecv = 0;
				do { 
					rec = recv(sockfd, buf + totalRecv, BUF_SIZE, 0); 
					totalRecv += rec; 
					if(totalRecv < 0)
					{
						serverActive = false;
						printf("Connection with server has been lost\n", buf);
						break;
					}
				} while (buf[totalRecv - 1] != '\0'); 

				if (totalRecv >= 0) {
					printf("%s\n", buf);
				}
			}
		}

		closesocket(sockfd); 
	}

	WSACleanup(); 
	return 0;
}