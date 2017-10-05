#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* HEY REMEMBER THESE THINGS
-MAKE A SPECIAL CASE IN SERVERRECVPACKET AND CLIENTRECVPACKET TO RECOGNISE PINGS
-MAKE THE VARIABLES NECESSARY FOR CLIENTSOCK AND SERVERSOCK TO SUPPORT PINGS
-CHANGE THE TYPE SECTION OF THE PROTOCOL TO AN 8-BIT VALUE
-REMOVE THE ID SYSTEM ENTIRELY
-MAKE ALL THE NECESSARY CHANGES TO SUPPORT THOSE PROTOCOL CHANGES
-MAYBE LOOK INTO HAVING SERVERRECVPACKET RECV FOR EACH CLIENT SO YOU DON'T HAVE TO DO IT IN YOUR PROGRAM
*/

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
	if (csock->lastvalue == SOCKET_ERROR) {
		return 5;
	};

	csock->alive = 1;

	return 0;
};

int CLIENTSENDPACKET(CLIENTSOCK *csock, char type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = type;

	strncpy(buffer + 1, csock->id, IDSIZE);
	strncpy(buffer + (1 + IDSIZE), data, DATASIZE);

	int result = send(csock->socket, buffer, PACKETSIZE, 0);
	if (result == SOCKET_ERROR) {
		return 1;
	};

	return 0;
};

int CLIENTRECVPACKET(CLIENTSOCK *csock, char *type, char *data) {
	char buffer[PACKETSIZE];
	char id[IDSIZE];

	int result = recv(csock->socket, buffer, PACKETSIZE, 0);
	if (result == 0 || result == SOCKET_ERROR) {
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

//SERVER
typedef struct SERVERSOCK SERVERSOCK;

struct SERVERSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	SOCKET clientlist[10]; //will probably change this later to a shadow array or something
	int clients = 0;
	int alive = 0;
	int lastvalue = 0;
};

int CREATESERVERSOCK(SERVERSOCK *ssock) {
	ssock->lastvalue = WSAStartup(MAKEWORD(2, 2), &ssock->wsa);
	if (ssock->lastvalue != 0) {
		return 1;
	};

	ssock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ssock->socket == INVALID_SOCKET) {
		return 2;
	};

	ssock->lastvalue = ioctlsocket(ssock->socket, FIONBIO, (u_long *) &NONBLOCKING);
	if (ssock->lastvalue == SOCKET_ERROR) {
		return 3;
	};

	ssock->server.sin_addr.s_addr = INADDR_ANY;
	ssock->server.sin_port = htons(PORT);
	ssock->server.sin_family = AF_INET;

	ssock->lastvalue = bind(ssock->socket, (struct sockaddr *) &ssock->server, sizeof(ssock->server));
	if (ssock->lastvalue == SOCKET_ERROR) {
		return 4;
	};

	ssock->alive = 1;

	return 0;
};

void ACCEPTCLIENTSOCK(SERVERSOCK *ssock) {
	listen(ssock->socket, 3);

	int serversize = sizeof(ssock->server);
	SOCKET tempsock = accept(ssock->socket, (struct sockaddr *) &ssock->server, &serversize);

	if (tempsock != INVALID_SOCKET) {
		ssock->clientlist[ssock->clients] = tempsock;

		send(tempsock, "ab", IDSIZE, 0);
		
		ssock->clients++;
	};
};

int SERVERSENDPACKET(SERVERSOCK *ssock, int clientnum, char type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = type;
	buffer[1] = 'a'; //i can cut this and the line below it later
	buffer[2] = 'b';

	strncpy(buffer + (1 + IDSIZE), data, DATASIZE);

	int result = send(ssock->clientlist[clientnum], buffer, PACKETSIZE, 0);
	if (result == SOCKET_ERROR) {
		return 1;
	}

	return 0;
};

int SERVERRECVPACKET(SERVERSOCK *ssock, int clientnum, char *type, char *data) {
	char buffer[PACKETSIZE];

	int result = recv(ssock->clientlist[clientnum], buffer, PACKETSIZE, 0);
	if (result == 0 || result == SOCKET_ERROR) {
		return 1;
	};

	type[0] = buffer[0];

	strncpy(data, buffer + (1 + IDSIZE), DATASIZE);

	return 0;
};

void REMOVESERVERSOCK(SERVERSOCK *ssock) {
	closesocket(ssock->socket);
	WSACleanup();
};
