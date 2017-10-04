#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

//CONSTANTS
const int PORT = 2127;
const char *IP = "127.0.0.1";
const int PACKETSIZE = 13;
const int IDSIZE = 2;
const int DATASIZE = PACKETSIZE - (IDSIZE + 1);
const u_long NONBLOCKING = 1; //this should always be above 0

//CLIENT
typedef struct CLIENTSOCK CLIENTSOCK;

struct CLIENTSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	char id[IDSIZE];
	int alive = 0;
	int lastvalue = 0;
};

int CREATECLIENTSOCK(CLIENTSOCK *csock) {
	csock->lastvalue = WSAStartup(MAKEWORD(2, 2), &csock->wsa);
	if (csock->lastvalue != 0) {
		return 1;
	};

	csock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (csock->socket == INVALID_SOCKET) {
		return 2;
	};

	csock->server.sin_addr.s_addr = inet_addr(IP);
	csock->server.sin_port = htons(PORT);
	csock->server.sin_family = AF_INET;

	csock->lastvalue = connect(csock->socket, (struct sockaddr *) &csock->server, sizeof(csock->server));
	if (csock->lastvalue == SOCKET_ERROR) {
		return 3;
	};

	char buffer[IDSIZE];
	csock->lastvalue = recv(csock->socket, buffer, IDSIZE, 0);
	if (csock->lastvalue <= 0) {
		return 4;
	};

	strncpy(csock->id, buffer, IDSIZE);

	csock->lastvalue = ioctlsocket(csock->socket, FIONBIO, (u_long *) &NONBLOCKING);

	csock->alive = 1;

	return 0;
};

int SENDCLIENTPACKET(CLIENTSOCK *csock, char type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = type;

	strncpy(buffer + 1, csock->id, IDSIZE);
	strncpy(buffer + (1 + IDSIZE), data, DATASIZE);

	int result = send(csock->socket, buffer, PACKETSIZE, 0);
	if (result <= 0) {
		return 1;
	};

	return 0;
};

int RECVCLIENTPACKET(CLIENTSOCK *csock, char *type, char *data) {
	char buffer[PACKETSIZE];
	char id[IDSIZE];

	int result = recv(csock->socket, buffer, PACKETSIZE, 0);
	if (result <= 0) {
		return 1;
	};

	type[0] = buffer[0];

	strncpy(id, buffer + 1, IDSIZE);
	if (strncmp(id, csock->id, IDSIZE) != 0) {
		return 2;
	};

	strncpy(data, buffer + (1 + IDSIZE), DATASIZE);

	return 0;
};

void REMOVECLIENTSOCK(CLIENTSOCK *csock) {
	closesocket(csock->socket);
	WSACleanup();
};