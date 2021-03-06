// ServerTCP.cpp: define el punto de entrada de la aplicación de consola.
//

#include "stdafx.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <vector>
#include <algorithm>

#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>

#define SERVER_PORT 12345
#define BUF_SIZE 4096
#define QUEUE_SIZE 10


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

std::vector<SOCKET> lstClients;

struct ThreadReturn
{
	int result;
};

struct SocketParam
{
	SOCKET socketId;
};


void * listenClients(void * param)
{
	ThreadReturn response { 0 };
	SOCKET socketCliId = ((SocketParam *)param)->socketId;
	std::string nick;
	std::string message;
	bool firstMessage = true;

	char buf[BUF_SIZE];
	while (response.result >= 0) {
		int rec = 0, totalRecv = 0;
		do {
			rec = recv(socketCliId, buf + totalRecv, BUF_SIZE, 0);
			totalRecv += rec;
			printf("rec:%d | totalRecv: %d\n", rec, totalRecv);
			if (totalRecv < 0) break;
		} while (buf[totalRecv - 1] != '\0');

		if (totalRecv < 0 || (totalRecv > 0 && !strcmp(buf, "q"))) {
			message.clear();
			message.append(nick).append(" abandoned the chat");
			printf("%s abandoned the chat", nick.c_str());

			pthread_mutex_lock(&mutex);
			for (auto socketClientDestiny : lstClients) {
				int numBytesSend, totalSend = 0;
				totalRecv = message.length() + 1;
				do {
					numBytesSend = send(socketClientDestiny, message.c_str(), totalRecv - totalSend, 0);
					totalSend += numBytesSend;
					if (totalSend < 0) {
						response.result = -1;
						break;
					}
				} while (totalSend < totalRecv);
			}
			// Se elimina el cliente que se ha desconectado de la lista
			lstClients.erase(std::remove(lstClients.begin(), lstClients.end(), socketCliId), lstClients.end());
			pthread_mutex_unlock(&mutex);
			if (totalRecv < 0) {
				closesocket(socketCliId);
			}
			response.result = -1;
			break;
		}

		if (!firstMessage) {
			message.clear();
			message.append(nick).append(": ").append(buf);

			pthread_mutex_lock(&mutex);
			for (auto socketClientDestiny : lstClients) {
				int numBytesSend, totalSend = 0;
				totalRecv = message.length() + 1;
				do {
					numBytesSend = send(socketClientDestiny, message.c_str(), totalRecv - totalSend, 0);
					totalSend += numBytesSend;
					if (totalSend < 0) {
						response.result = -1;
						break;
					}
				} while (totalSend < totalRecv);
			}
			pthread_mutex_unlock(&mutex);
		} else {
			nick = buf;
			firstMessage = false;
		}
	}

	pthread_exit(NULL);
	return &response;
}

void closeAllClients()
{
	for(auto socketCliId : lstClients)
		closesocket(socketCliId); 
}

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	int wsaerr;
	int on = 1;
	struct sockaddr_in addr;
	struct sockaddr_in client;
	SOCKET socketCliId;
	SOCKET sockId;

	char buf[BUF_SIZE];
	wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaerr != 0) {
		printf("Could not initialize Winsock.\n");
		return -1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET; // Aquí NO se usa htons.
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(SERVER_PORT);
	sockId = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockId < 0) {
		printf("Error creating socket\n");
		WSACleanup();
		return -1;
	}

	setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
	if (bind(sockId, (struct sockaddr *) &addr, sizeof(addr)) < 0) { 
		printf("Error binding port\n"); 
		WSACleanup(); 
		return -1;
	} 

	if (listen(sockId, QUEUE_SIZE) < 0) { 
		printf("listen failed\n"); 
		WSACleanup(); return -1; 
	} 

	at_quick_exit (closeAllClients);

	while (1) {
		socklen_t sock_len = sizeof(client); 
		memset(&client, 0, sizeof(client)); 
		socketCliId = accept(sockId, (struct sockaddr *)&client, &sock_len); 
		if (socketCliId == INVALID_SOCKET) printf("Can't accept client:%d\n", WSAGetLastError()); 
		else { 
			pthread_mutex_lock(&mutex);
			pthread_t tid;
			SocketParam param { socketCliId };
			if (!pthread_create(&tid, nullptr, listenClients, &param)) {
				lstClients.push_back(socketCliId);
			}
			pthread_mutex_unlock(&mutex);
		}
	} 

	quick_exit(EXIT_SUCCESS);
	
	closesocket(sockId); 
	WSACleanup();

	return 0;
}
