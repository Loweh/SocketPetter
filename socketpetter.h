#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* HEY REMEMBER THESE THINGS
-MAKE THINGS A BIT MORE COHERENT AND NICE LOOKING, FIX STRUCTS AND NAMES
-CONSIDER HIDING THE FOR LOOPS (SERVER-SIDE) IN THE FUNCTIONS THEMSELVES
-AS A RESULT OF THE PREVIOUS POINT, CONSIDER FUSING HANDLEPING WITH RECV
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
	time_t lastpingrecv;
	time_t lastpingsent;
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

	csock->lastpingrecv = currenttime;
	csock->lastpingsent = currenttime;

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

		csock->lastpingrecv = currenttime;
	};

	strncpy(data, buffer + 1, DATASIZE);

	return 0;
};

void CLIENTHANDLEPING(CLIENTSOCK *csock) {
	int difference = 0;
	time_t currenttime;

	time(&currenttime);

	difference = difftime(currenttime, csock->lastpingrecv);
	if (difference == PINGTIMEOUT) {
		csock->alive = 0;
	};

	difference = difftime(currenttime, csock->lastpingsent);
	if (difference == (PINGTIMEOUT - 2)) { //magic number to give some time for the server to get the ping
		char buffer[PACKETSIZE];
		buffer[0] = (char) PINGTYPEVALUE;

		send(csock->socket, buffer, PACKETSIZE, 0);
		time(&csock->lastpingsent);
	};
};

void REMOVECLIENTSOCK(CLIENTSOCK *csock) {
	closesocket(csock->socket);
	WSACleanup();
};

//SERVER
typedef struct SERVERCLIENT SERVERCLIENT;

struct SERVERCLIENT {
	SOCKET socket;
	time_t lastpingrecv;
	time_t lastpingsent;
	int alive = 1;
};

typedef struct SERVERSOCK SERVERSOCK;

struct SERVERSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	SERVERCLIENT clientlist[10]; //will probably change this later to a shadow array or something
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

	SERVERCLIENT client;
	client.socket = tempsock;

	time_t currenttime;
	time(&currenttime);
	client.lastpingrecv = currenttime;
	client.lastpingsent = currenttime;


	if (tempsock != INVALID_SOCKET) {
		ssock->clientlist[ssock->clients] = client;
		
		ssock->clients++;
	};
};

int SERVERSENDPACKET(SERVERSOCK *ssock, int clientnum, unsigned int type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = (char) type;

	strncpy(buffer + 1, data, DATASIZE);

	int result = send(ssock->clientlist[clientnum].socket, buffer, PACKETSIZE, 0);
	if (result == SOCKET_ERROR) {
		return 1;
	}

	return 0;
};

int SERVERRECVPACKET(SERVERSOCK *ssock, int clientnum, unsigned int *type, char *data) {
	char buffer[PACKETSIZE];

	int result = recv(ssock->clientlist[clientnum].socket, buffer, PACKETSIZE, 0);
	if (result == 0 || result == SOCKET_ERROR) {
		return 1;
	};

	*type = (unsigned int) buffer[0];
	if (*type == PINGTYPEVALUE) {
		time_t currenttime;
		time(&currenttime);

		ssock->clientlist[clientnum].lastpingrecv = currenttime;
	};

	strncpy(data, buffer + 1, DATASIZE);

	return 0;
};

void SERVERHANDLEPING(SERVERSOCK *ssock, int clientnum) {
	int difference = 0;
	time_t currenttime;

	time(&currenttime);

	difference = difftime(currenttime, ssock->clientlist[clientnum].lastpingrecv);
	if (difference == PINGTIMEOUT && ssock->clientlist[clientnum].alive) {
		printf("%i is dead\n", clientnum);

		ssock->clientlist[clientnum].alive = 0;
		closesocket(ssock->clientlist[clientnum].socket);
	};

	difference = difftime(currenttime, ssock->clientlist[clientnum].lastpingsent);
	if (difference == (PINGTIMEOUT - 2)) { //magic number to give the ping to the client some time to get there
		char buffer[PACKETSIZE];
		buffer[0] = 'd';

		send(ssock->clientlist[clientnum].socket, buffer, PACKETSIZE, 0);
		time(&ssock->clientlist[clientnum].lastpingsent);
	};
};

void REMOVESERVERSOCK(SERVERSOCK *ssock) {
	closesocket(ssock->socket);
	WSACleanup();
};
