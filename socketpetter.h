#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* HEY REMEMBER THESE THINGS
-MAKE PINGS NOT SO UGLY (PROBABLY BY MAKING FUNCTIONS FOR SENDING PINGS AND CHECKING FOR THEM GOING OVER THE TIMEOUT
*/

//CONSTANTS
const int PORT = 2127;
const char *IP = "127.0.0.1";
const int PACKETSIZE = 13;
const int DATASIZE = PACKETSIZE - 1;
const u_long NONBLOCKING = 1; //this should always be above 0
const int PINGTYPEVALUE = 100;
const int PINGTIMEOUT = 30;

//CLIENT
typedef struct CLIENTSOCK CLIENTSOCK;

struct CLIENTSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	time_t lastping;
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

	csock->lastvalue = ioctlsocket(csock->socket, FIONBIO, (u_long *) &NONBLOCKING);
	if (csock->lastvalue == SOCKET_ERROR) {
		return 5;
	};

	time_t currenttime;
	time(&currenttime);

	csock->lastping = currenttime;

	csock->alive = 1;

	return 0;
};

int CLIENTSENDPACKET(CLIENTSOCK *csock, unsigned int type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = (char) type;

	strncpy(buffer + 1, data, DATASIZE);

	int result = send(csock->socket, buffer, PACKETSIZE, 0);
	if (result == SOCKET_ERROR) {
		return 1;
	};

	return 0;
};

int CLIENTRECVPACKET(CLIENTSOCK *csock, unsigned int *type, char *data) {
	char buffer[PACKETSIZE];

	int result = recv(csock->socket, buffer, PACKETSIZE, 0);
	if (result == 0 || result == SOCKET_ERROR) {
		return 1;
	};

	*type = (unsigned int) buffer[0];

	if (*type == PINGTYPEVALUE) {
		time_t currenttime;
		time(&currenttime);

		csock->lastping = currenttime;
	};

	strncpy(data, buffer + 1, DATASIZE);

	return 0;
};

void REMOVECLIENTSOCK(CLIENTSOCK *csock) {
	closesocket(csock->socket);
	WSACleanup();
};

//SERVER
typedef struct SERVERENDCLIENTSOCK SERVERENDCLIENTSOCK;

struct SERVERENDCLIENTSOCK {
	SOCKET sock;
	time_t lastping;
	int dead = 0;
};

typedef struct SERVERSOCK SERVERSOCK;

struct SERVERSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	SERVERENDCLIENTSOCK clientlist[10]; //will probably change this later to a shadow array or something
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

	SERVERENDCLIENTSOCK client;
	client.sock = tempsock;

	time_t currenttime;
	time(&currenttime);
	client.lastping = currenttime;


	if (tempsock != INVALID_SOCKET) {
		ssock->clientlist[ssock->clients] = client;
		
		ssock->clients++;
	};
};

int SERVERSENDPACKET(SERVERSOCK *ssock, int clientnum, unsigned int type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = (char) type;

	strncpy(buffer + 1, data, DATASIZE);

	int result = send(ssock->clientlist[clientnum].sock, buffer, PACKETSIZE, 0);
	if (result == SOCKET_ERROR) {
		return 1;
	}

	return 0;
};

int SERVERRECVPACKET(SERVERSOCK *ssock, int clientnum, unsigned int *type, char *data) {
	char buffer[PACKETSIZE];

	int result = recv(ssock->clientlist[clientnum].sock, buffer, PACKETSIZE, 0);
	if (result == 0 || result == SOCKET_ERROR) {
		return 1;
	};

	*type = (unsigned int) buffer[0];
	if (*type == PINGTYPEVALUE) {
		time_t currenttime;
		time(&currenttime);

		ssock->clientlist[clientnum].lastping = currenttime;
	};

	strncpy(data, buffer + 1, DATASIZE);

	return 0;
};

void REMOVESERVERSOCK(SERVERSOCK *ssock) {
	closesocket(ssock->socket);
	WSACleanup();
};
